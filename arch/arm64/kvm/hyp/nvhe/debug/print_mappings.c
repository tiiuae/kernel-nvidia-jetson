
#include <linux/kernel.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_host.h>
#include <asm/kvm_pgtable.h>

#include <hyp/hyp_print.h>
#include <nvhe/mem_protect.h>
#include <nvhe/pkvm.h>

extern struct host_mmu host_mmu;
extern struct kvm_pgtable pkvm_pgtable;

char *parse_attrs(char *p, uint64_t attrs, uint64_t stage);
struct pkvm_hyp_vm *get_vm_by_handle(pkvm_handle_t handle);

struct dbg_map_data {
	u64 vaddr;
	u64 pte;
	u32 level;
};

struct walk_data {
	u32 	stage;
	u64 	size;
	u64 	start_ipa;
	u64 	start_phys;
	u64 	attr;
	u64 	host_attr;
	u32	level;
};

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

static void print_databuf(struct walk_data *data)
{
	char buff[128];
	char *type;

	switch (data->level) {
	case 0:
		type = "512G block";
		break;
	case 1:
		type = "1G block ";
		break;
	case 2:
		type = "2M block";
		break;
	case 3:
		type = "4K page";
		break;
	default:
		type = "fail";
	}
	parse_attrs(buff, data->attr, data->stage);

	hyp_dbg_print("%02d pages 0x%llx -> 0x%llx %s\n", data->size,
			data->start_ipa, data->start_phys, buff);
}
static void clean_databuf(struct walk_data *data)
{
	data->size = 0;
}

static void init_databuf(struct walk_data *data, u64 addr, u32 level, kvm_pte_t *ptep)
{
	data->size = 1;
	data->level = level;
	data->start_ipa = addr;
	data->attr = (*ptep) & ~KVM_PTE_ADDR_MASK;
	data->start_phys = (*ptep) & KVM_PTE_ADDR_MASK;
}

static int update_databuf(struct walk_data *data, u64 addr, u32 level, kvm_pte_t *ptep)
{
	if (data->size == 0) {
		init_databuf(data, addr, level, ptep);
		return 1;
	}
	if ((data->attr == ((*ptep) & ~KVM_PTE_ADDR_MASK)) &&
	    (data->level == level)  &&

	    ((*ptep) & KVM_PTE_ADDR_MASK) == data->start_phys + data->size * (1UL << bit_shift(level))) {
		data->size++;
		return 1;
	}
	return 0;
}
static int print_mapping_walker(u64 addr, u64 end, u32 level, kvm_pte_t *ptep,
		       enum kvm_pgtable_walk_flags flag,
		       void * const arg)
{
	struct walk_data *data = arg;

	if (flag == KVM_PGTABLE_WALK_LEAF) {
		if ((*ptep) & KVM_PTE_ADDR_MASK) {
			if (update_databuf(data, addr, level, ptep))
				return 0;

			print_databuf(data);
			init_databuf(data, addr, level, ptep);

		} else {
			if (data->size) {
				print_databuf(data);
				clean_databuf(data);
			}
		}
	}
	if (flag == KVM_PGTABLE_WALK_TABLE_POST) {
		if (data->size) {
			print_databuf(data);
			clean_databuf(data);
		}
	}

	return 0;
}

int print_mappings(u32 id, u64 addr, u64 size)
{
	struct kvm_pgtable *pgt;
	struct pkvm_hyp_vm *vm;

	struct walk_data wdata = {
			.size = 0,
	};
	struct kvm_pgtable_walker walker_s2 = {
		.cb	= print_mapping_walker,
		.flags	= KVM_PGTABLE_WALK_LEAF |
			  KVM_PGTABLE_WALK_TABLE_POST,
		.arg	= &wdata,
	};

	if (id == 0) {
		wdata.stage = 1;
		pgt = &pkvm_pgtable;
	} else if (id == 1) {
		wdata.stage = 2;
		pgt = host_mmu.arch.mmu.pgt;
	} else {
		wdata.stage = 2;
		vm = get_vm_by_handle(id - 2 + 0x1000);
		if (!vm)
			return -EINVAL;
		pgt = &vm->pgt;
	}

	hyp_print("print_mappings id %x addr %llx, size %llx\n", id, addr, size);

	return kvm_pgtable_walk(pgt, addr, size, &walker_s2);
}

