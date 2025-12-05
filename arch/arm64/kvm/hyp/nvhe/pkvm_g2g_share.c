// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kvm_host.h>
#include <nvhe/pkvm.h>
#include <kvm/arm_hypercalls.h>
#include <nvhe/mem_protect.h>
#include <nvhe/memory.h>
#include <nvhe/spinlock.h>
#include <nvhe/pkvm_g2g_share.h>

enum share_mode {NONE, INITIATOR, COMPLETER};
enum g2g_share_status {EMPTY = 0, INITIATED, COMPLETED, INIT_UNSHARED, COMP_UNSHARED};

struct g2g_share {
	pkvm_handle_t initiator_handle;
	pkvm_handle_t completer_handle;
	unsigned long initiator_ipa;
	unsigned long completer_ipa;
	u32	page_nr;
	enum g2g_share_status status;
};

struct g2g_pool {
	/*The size of the struct g2g_share array depends on how many pages from
	 * the host are reserved for guest to guest sharing. Each page needs one
	 * g2g_share structure.
	 */
	struct g2g_share (*shares)[];
	/* number of pages reserved for sharing */
	u32 nr_pages;
	/* memory allocated for g2g sharing. guests' IPA addresses are s2-mapped
	 * here.
	 * Please note that the actual number of pages to be shared will be
	 * slightly less than the number allocated by the host.
	 */
	void  *shared_mem;
};

static struct g2g_pool g2g_pool;

/* g2g sharing are functions are common to all cores, only one core
 * can use them at a time.
 * Inside this lock is another spin lock, vm>pgtable_lock.
 */
static DEFINE_HYP_SPINLOCK(g2g_share_lock);

int pkvm_init_g2g_pool(void)
{
	int hdr_pages;
	void *base = hyp_phys_to_virt(g2g_share_base);
	int total_pages = g2g_share_size / PAGE_SIZE;

	if (!PAGE_ALIGNED(g2g_share_base) || !PAGE_ALIGNED(g2g_share_size))
		return -EINVAL;

	hdr_pages = DIV_ROUND_UP(sizeof(struct g2g_share) * total_pages +
				 sizeof(u32), PAGE_SIZE);
	if (hdr_pages >= total_pages)
		return -EINVAL;

	memset((void *) base, 0, g2g_share_size);
	g2g_pool.shares = (struct g2g_share(*)[]) base;
	g2g_pool.nr_pages = total_pages - hdr_pages;
	g2g_pool.shared_mem = (void *) base + hdr_pages * PAGE_SIZE;

	return 0;
}

static phys_addr_t get_share_phys(int id)
{
	return hyp_virt_to_phys(g2g_pool.shared_mem + PAGE_SIZE * id);
}

int get_new_share(void)
{
	int i;
	struct g2g_share *share;

	if (!g2g_pool.shares)
		return -EINVAL;

	for (i = 0; i < g2g_pool.nr_pages; i++) {
		share = &(*g2g_pool.shares)[i];
		if (share->status == EMPTY)
			return i;
	}
	return -EINVAL;
}

enum share_mode get_g2g_mode(struct g2g_share *share,
			     pkvm_handle_t handle, pkvm_handle_t partner, u64 ipa)
{
	if (share->status == EMPTY)
		return NONE;

	if (share->initiator_handle == handle)
		if ((!ipa) || (share->initiator_ipa == ipa))
			if ((!partner) || (share->completer_handle == partner))
				return INITIATOR;

	if (share->completer_handle == handle)
		if ((!ipa) || (share->completer_ipa == ipa))
			if ((!partner) || (share->initiator_handle == partner))
				return COMPLETER;
	return NONE;
}

pkvm_handle_t find_next_g2g_share(struct pkvm_hyp_vm *hyp_vm, pkvm_handle_t partner)
{
	struct g2g_share *share;
	pkvm_handle_t guest = hyp_vm->kvm.arch.pkvm.handle;
	pkvm_handle_t handle;
	int idx;
	int share_id;
	int start;

	if (partner == 0)
		start = 0;
	else
		start = vm_handle_to_idx(partner) + 1;

	/* search the shared table for existing VMIDs with which we have a share */
	for (idx = start; idx < KVM_MAX_PVMS; idx++) {
		handle = idx_to_vm_handle(idx);
		if (handle == guest)
			continue;
		for (share_id = 0; share_id < g2g_pool.nr_pages; share_id++) {
			share = &(*g2g_pool.shares)[share_id];
			if (get_g2g_mode(share, guest, handle, 0) != NONE)
				return handle;

		}
	}

	return 0;
}

static int do_g2g_map(struct pkvm_hyp_vcpu *vcpu, u64 ipa, phys_addr_t phys)
{
	struct kvm_hyp_memcache *mc = &vcpu->vcpu.arch.stage2_mc;
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	enum kvm_pgtable_prot prot;
	int ret;

	guest_lock_component(vm);
	prot = pkvm_mkstate(KVM_PGTABLE_PROT_RW, PKVM_PAGE_SHARED_BORROWED);
	ret = kvm_pgtable_stage2_map(&vm->pgt, ipa, PAGE_SIZE, phys, prot, mc, 0);
	guest_unlock_component(vm);
	return  ret;
}

bool pkvm_g2g_share(struct pkvm_hyp_vcpu *hyp_vcpu, u64 *exit_code)
{
	struct pkvm_hyp_vm *hyp_vm = pkvm_hyp_vcpu_to_hyp_vm(hyp_vcpu);
	struct kvm_vcpu *vcpu = &hyp_vcpu->vcpu;
	struct g2g_share *share;
	pkvm_handle_t handle = hyp_vm->kvm.arch.pkvm.handle;
	bool share_completed = false;
	int share_id = 0;
	int ret = SMCCC_RET_SUCCESS;
	u64 ipa = smccc_get_arg1(vcpu);
	u32 page_nr = smccc_get_arg2(vcpu) + 1;
	u64 partner = smccc_get_arg3(vcpu);

	hyp_spin_lock(&g2g_share_lock);

	if (handle == partner) {
		ret = -EINVAL;
		goto err;
	}

	if (!g2g_pool.shares) {
		if (pkvm_init_g2g_pool()) {
			ret = -EINVAL;
			goto err;
		}
	}

	/* Ensure that there are no duplicate IPA address of the guest */
	for (share_id = 0; share_id < g2g_pool.nr_pages; share_id++) {
		share = &(*g2g_pool.shares)[share_id];
		if (get_g2g_mode(share, handle, 0, ipa) != NONE) {
			ret = -EEXIST;
			goto err;
		}
	}

	/* look for an existing share request for this guest */
	for (share_id = 0; share_id < g2g_pool.nr_pages; share_id++) {
		share = &(*g2g_pool.shares)[share_id];
		if ((share->page_nr == page_nr) && (share->status != EMPTY)) {
			switch (get_g2g_mode(share, handle, partner, 0)) {
			case INITIATOR:
				if ((share->status == INITIATED) ||
				    (share->status == COMPLETED) ||
				    (share->status == COMP_UNSHARED)) {
					ret = -EEXIST;
					goto err;
				}
				break;

			case COMPLETER:
				if (share->status != INITIATED) {
					ret = -EEXIST;
					goto err;
				}
				ret = do_g2g_map(hyp_vcpu, ipa, get_share_phys(share_id));
				if (!ret) {
					share->completer_ipa = ipa;
					share->status = COMPLETED;
					share_completed = true;
				}
				/* share complete */
				goto out;

			case NONE:
				continue;
			default:
			}
		}
	}

	/* No share request for this guest found, create it */
	share_id = get_new_share();
	if (share_id < 0) {
		ret = -ENOSPC;
		goto err;
	}

	ret = do_g2g_map(hyp_vcpu, ipa, get_share_phys(share_id));
	if (!ret) {
		share = &(*g2g_pool.shares)[share_id];
		share->completer_handle = partner;
		share->page_nr = page_nr;
		share->initiator_handle = handle;
		share->initiator_ipa = ipa;
		share->status = INITIATED;
	}

out:
	if (ret == -ENOMEM) {
		/* do_g2g_map() tried to allocate memory for new page table
		 * without success.
		 */
		ret = pkvm_handle_empty_memcache(hyp_vcpu, exit_code);
		if (ret) {
			/* handle memcache request failed, nothing can be done here */
			ret = -EINVAL;
			goto err;
		}
		hyp_spin_unlock(&g2g_share_lock);
		/* Go to the host and do memcache request.
		 * After that pkvm calls pkvm_g2g_share() again.
		 */
		return false;
	}
err:
	hyp_spin_unlock(&g2g_share_lock);
	smccc_set_retval(vcpu, ret, share_completed, 0, 0);
	return true;

}

bool pkvm_g2g_share_query(struct pkvm_hyp_vcpu *hyp_vcpu, u64 *exit_code)
{
	struct pkvm_hyp_vm *hyp_vm = pkvm_hyp_vcpu_to_hyp_vm(hyp_vcpu);
	struct kvm_vcpu *vcpu = &hyp_vcpu->vcpu;
	pkvm_handle_t partner = 0;
	pkvm_handle_t handle = hyp_vm->kvm.arch.pkvm.handle;
	struct g2g_share *share;
	int share_id = 0;
	enum share_mode mode;
	u32 free = 0;
	u32 waiting = 0;
	u32 completed = 0;
	u32 incoming_req = 0;
	u32 unsharing = 0;
	u64 stat1 = 0;
	u64 stat2 = 0;
	u64 stat3 = 0;
	int ret = 0;

	hyp_spin_lock(&g2g_share_lock);
	if (!g2g_pool.shares)
		goto out;

	partner = smccc_get_arg1(vcpu);
	for (share_id = 0; share_id < g2g_pool.nr_pages; share_id++) {
		share = &(*g2g_pool.shares)[share_id];
		if (share->status == EMPTY)
			free++;
		mode = get_g2g_mode(share, handle, partner, 0);
		if (mode == NONE)
			continue;
		switch (share->status) {
		case COMPLETED:
			completed++;
			break;
		case INITIATED:
			if (mode == INITIATOR)
				waiting++;
			if (mode == COMPLETER)
				incoming_req++;
			break;
		case INIT_UNSHARED:
			if (mode == COMPLETER)
				unsharing++;
			else
				waiting++;
			break;
		case COMP_UNSHARED:
			if (mode == INITIATOR)
				unsharing++;
			else
				waiting++;
			break;
		default:
		}
	}

	partner = find_next_g2g_share(hyp_vm, partner);
out:
	stat1 = (u64) incoming_req << 32 | (waiting & 0xffffffffUL);
	stat2 = (u64) completed << 32 | (unsharing & 0xffffffffUL);
	stat3 = (u64) (partner & 0xffffUL) << 48  |
		      (handle & 0xffffUL) << 32 |
		      (free & ((1UL << 32) - 1));

	hyp_spin_unlock(&g2g_share_lock);
	smccc_set_retval(vcpu, ret, stat1, stat2, stat3);

	return true;
}


int do_pkvm_g2g_unmap(struct pkvm_hyp_vm *vm, u64 ipa)
{
	int ret;

	guest_lock_component(vm);
	ret = kvm_pgtable_stage2_unmap(&vm->pgt, ipa, 4096);
	guest_unlock_component(vm);

	return ret;
}

static int __pkvm_g2g_unshare(struct pkvm_hyp_vm *hyp_vm, pkvm_handle_t handle,
			      u64 ipa, u32 *unmapped)
{
	struct g2g_share *share;
	u64 unmap_ipa = 0;
	int share_id;
	u64 phys;
	int ret = 0;

	hyp_spin_lock(&g2g_share_lock);
	if (!g2g_pool.shares) {
		/* Nothing to unshare */
		ret = -EINVAL;
		goto err;
	}

	if (unmapped)
		*unmapped = 0;

	for (share_id = 0; share_id < g2g_pool.nr_pages; share_id++) {
		share = &(*g2g_pool.shares)[share_id];
		switch (get_g2g_mode(share, handle, 0, ipa)) {
		case INITIATOR:
			unmap_ipa = share->initiator_ipa;
			share->initiator_ipa = 0;
			if ((share->status == INITIATED) ||
			    (share->status == COMP_UNSHARED))
				share->status = EMPTY;
			else
				share->status = INIT_UNSHARED;
			break;

		case COMPLETER:
			unmap_ipa = share->completer_ipa;
			share->completer_ipa = 0;
			if (share->status == INIT_UNSHARED)
				share->status = EMPTY;
			else
				share->status = COMP_UNSHARED;
			break;

		case NONE:
			continue;
		default:
		}

		if (unmap_ipa) {
			if (share->status == EMPTY) {
				/* both guests have unmapped this */
				phys = get_share_phys(share_id);
				memset(hyp_phys_to_virt(phys), 0, PAGE_SIZE);
			}
			ret = do_pkvm_g2g_unmap(hyp_vm, unmap_ipa);
			/* What we can do if unmap fails */
			if (ret)
				goto err;

			/* count unmapped pages */
			if (unmapped)
				(*unmapped)++;
			if (ipa) {
				/* if an IPA address is given, only that address
				 * will be unmapped, otherwise all addresses
				 * shared by the guest will be unmapped
				 */
				break;
			}
			unmap_ipa = 0;
		}
	}
err:
	hyp_spin_unlock(&g2g_share_lock);

	return ret;
}

bool pkvm_g2g_unshare(struct pkvm_hyp_vcpu *hyp_vcpu, u64 *exit_code)
{
	struct pkvm_hyp_vm *hyp_vm = pkvm_hyp_vcpu_to_hyp_vm(hyp_vcpu);
	struct kvm_vcpu *vcpu = &hyp_vcpu->vcpu;
	pkvm_handle_t handle = hyp_vm->kvm.arch.pkvm.handle;
	u64 ipa = smccc_get_arg1(vcpu);
	u32 unmapped = 0;
	int ret;


	ret = __pkvm_g2g_unshare(hyp_vm, handle, ipa, &unmapped);

	smccc_set_retval(vcpu, ret, unmapped, 0, 0);

	return true;
}

void pkvm_g2g_share_teardown(pkvm_handle_t handle)
{
	struct pkvm_hyp_vm *hyp_vm = get_vm_by_handle(handle);

	__pkvm_g2g_unshare(hyp_vm, handle, 0, 0);
}
