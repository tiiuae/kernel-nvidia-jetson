#include <linux/kernel.h>
#include <asm/kvm_mmu.h>

#include <hyp/hyp_print.h>
#include <nvhe/mem_protect.h>
#include <nvhe/pkvm.h>
#include <asm/kvm_host.h>

#define S2_XN_SHIFT	53
#define S2_XN_MASK	(0x3UL << S2_XN_SHIFT)
#define S2AP_SHIFT	6
#define S2AP_MASK	(0x3UL << S2AP_SHIFT)
#define S2_EXEC_NONE	(0x2UL << S2_XN_SHIFT)
#define S2AP_READ	(1UL << S2AP_SHIFT)

#define tlbi_el1_ipa(va)                                                       \
	do {                                                                   \
		__asm__ __volatile__("mov	x20, %[vaddr]\n"               \
				     "lsr	%[vaddr], %[vaddr], #12\n"     \
				     "tlbi	ipas2e1is, %[vaddr]\n"         \
				     "mov	%[vaddr], x20\n"               \
				     :                                         \
				     : [vaddr] "r"(va)                         \
				     : "memory", "x20");                       \
	} while (0);


extern struct host_mmu host_mmu;
extern struct kvm_pgtable pkvm_pgtable;

struct pkvm_hyp_vm *get_vm_by_handle(pkvm_handle_t handle);

struct data_buf {
	u64 	size;
	u64 	start_ipa;
	u64 	start_phys;
	u64 	attr;
	u64 	host_attr;
	u32	level;
};

struct count_shares_data {
	struct kvm_pgtable *host_pgt;
	struct data_buf data;
	u32 share_count;
	bool lock;
	bool hyp;
};
struct host_data {
	u64 	pte;

};
struct walker_data {
	u64	phys;
	u64 	*ptep;

};

typedef struct kvm_pgtable kvm_pgtable_t;

char *parse_attrs(char *p, uint64_t attrs, uint64_t stage);

static int host_walker(u64 addr, u64 end, u32 level, kvm_pte_t *ptep,
		       enum kvm_pgtable_walk_flags flag,
		       void * const arg)
{
	struct host_data *data = arg;

	data->pte = *ptep;
	return 1;
}

static int bit_shift(u32 level)
{
	int shift;

	switch (level) {
	case 0:
		shift = 39;
		break;
	case 1:
		shift = 30;
		break;
	case 2:
		shift = 21;
		break;
	case 3:
		shift = 12;
		break;
	default:
		shift = 0;
	}
	return shift;
}


#define SHARE_BIT_SHIFT 55
#define SHARE_BIT_MASK  (0b1111UL << SHARE_BIT_SHIFT)
#define GET_SHARE_BITS(x) ((x & SHARE_BIT_MASK) >> SHARE_BIT_SHIFT)

static void clean_dbuf(struct data_buf *data)
{
	memset(data, 0, sizeof(struct data_buf));
}

static void init_dbuf(struct data_buf *data, u64 addr, u32 level,
		      kvm_pte_t *ptep, u64 host_attr)
{
	data->size = 1;
	data->level = level;
	data->start_ipa = addr;
	data->attr = (*ptep) & ~KVM_PTE_ADDR_MASK;
	data->host_attr = host_attr;
	data->start_phys = (*ptep) & KVM_PTE_ADDR_MASK;
}

static int update_dbuf(struct data_buf *data, u64 addr, u32 level,
		       kvm_pte_t *ptep, u64 host_attr)
{
	if (data->size == 0) {
		init_dbuf(data, addr, level, ptep, host_attr);
		return 1;
	}
	if ((data->attr == ((*ptep) & ~KVM_PTE_ADDR_MASK)) &&
	    (data->level == level)  &&
	    ((*ptep) & KVM_PTE_ADDR_MASK) == data->start_phys +
	     data->size * (1UL << bit_shift(level))) {
		data->size++;
		return 1;
	}
	return 0;
}
static void print_header(void)
{
	hyp_dbg_print("Count     IPA             Phys        page attributes");
	hyp_dbg_print("                host page attributes\n");
	hyp_dbg_print("                                      prv unp type \n");
}

static void print_dbuf(struct data_buf *data, bool hyp)
{
	char buff[128];

	hyp_dbg_print("%2d pages %#014llx  %#012llx ", data->size,
			data->start_ipa, data->start_phys);
	hyp_dbg_print("%s ", parse_attrs(buff, data->attr, hyp ? 1 : 2));
	hyp_dbg_print(": %s\n", parse_attrs(buff, data->host_attr, 2));
}

static int count_shares_walker(u64 addr, u64 end, u32 level, kvm_pte_t *ptep,
		       enum kvm_pgtable_walk_flags flag,
		       void * const arg)
{
	u64 phys = (*ptep) & KVM_PTE_ADDR_MASK;
	struct host_data hwalker_data;
	struct count_shares_data *data = arg;
	struct kvm_pgtable_walker walker = {
		.cb	= host_walker,
		.flags	= KVM_PGTABLE_WALK_LEAF,
		.arg	= &hwalker_data,
	};

	if (phys == 0) {
		return 0;
	}

	memset(&hwalker_data, 0, sizeof(hwalker_data));
	kvm_pgtable_walk(data->host_pgt, phys, 0x01000, &walker);

	if (hwalker_data.pte & KVM_PTE_ADDR_MASK) {
		data->share_count++;
		if (data->lock) {
			hyp_print("lock %llx\n", *ptep);
			*ptep &= ~(S2_XN_MASK & S2AP_MASK);
			*ptep |= (S2_EXEC_NONE | S2AP_READ);
			hyp_print("lock2 %llx\n", *ptep);

			dsb(ish);
			isb();
			tlbi_el1_ipa(phys);
			dsb(ish);
			isb();
		}

		if (update_dbuf(&data->data, addr, level, ptep,
				hwalker_data.pte))
			return 0;

		print_dbuf(&data->data, data->hyp);
		init_dbuf(&data->data, addr, level, ptep, hwalker_data.pte);
	} else {
		if (data->data.size) {
			print_dbuf(&data->data, data->hyp);
			clean_dbuf(&data->data);
		}
	}

	return 0;
}

int count_shared(pkvm_handle_t handle, u64 size, u64 lock)

{
	int ret;
	struct kvm_pgtable *guest_pgt;
	struct pkvm_hyp_vm *vm;
	struct count_shares_data walker_data;
	struct kvm_pgtable_walker walker = {
		.cb	= count_shares_walker,
		.flags	= KVM_PGTABLE_WALK_LEAF,
		.arg	= &walker_data,
	};

	if (size == 0)
		size = (1UL << 48) - 1;

	hyp_print("count_shared id: %x size: %llx %d\n", handle, size, lock);

	memset(&walker_data, 0, sizeof(walker_data));
	walker_data.host_pgt = host_mmu.arch.mmu.pgt;
	walker_data.lock = (bool) lock;
	if (handle) {
		vm = get_vm_by_handle(handle);
		if (!vm) {
			hyp_print("no handle\n");
			return -EINVAL;
		};
		walker_data.hyp = false;
		guest_pgt = &vm->pgt;
	} else {
		/* hyperpevisor mapping */
		walker_data.hyp = true;
		guest_pgt = &pkvm_pgtable;
	}
	print_header();

	ret = kvm_pgtable_walk(guest_pgt, 0, size, &walker);
	hyp_dbg_print("shared count %d\n", walker_data.share_count);
	return 0;
}

