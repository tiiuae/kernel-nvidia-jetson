/*
 * print_mappings.c
 *
 *  Created on: Mar 22, 2024
 *      Author: mt
 */
#include <linux/kernel.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_host.h>
#include <asm/kvm_pgtable.h>

#include <hyp/hyp_print.h>
#include <nvhe/mem_protect.h>
#include <nvhe/pkvm.h>
#define ATTR_MASK 0xffff000000000fffUL

extern struct host_mmu host_mmu;
extern struct kvm_pgtable pkvm_pgtable;

char *parse_attrs(char *p, uint64_t attrs, uint64_t stage);
struct pkvm_hyp_vm *get_vm_by_handle(pkvm_handle_t handle);

struct dbg_map_data {
	u64 vaddr;
	u64 pte;
	u32 level;
};

struct s2_walk_data {
	u64	phys;
	u64 	start_ipa;
	u64 	start_phys;
	u64 	size;
	u64 	attr;
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

static int print_entry(struct s2_walk_data *data, kvm_pte_t *ptep, u32 level, u64 addr)
{
	char attrbuf[128];
	char *type;

	switch (data->level) {
	case 0: type = "512G block";
	break;
	case 1: type = "1G block ";
	break;
	case 2: type = "2M block";
	break;
	case 3: type = "4K page";
	break;
	default:
		type = "fail";
	}
	parse_attrs(attrbuf, data->attr, 2);
	hyp_dbg_print("0x%llx -> 0x%llx %s %d * %s %llx\n",data->start_ipa,
			data->start_phys, attrbuf, data->size, type);
	return 0;
}

static int print_mapping_walker(u64 addr, u64 end, u32 level, kvm_pte_t *ptep,
		       enum kvm_pgtable_walk_flags flag,
		       void * const arg)
{
	struct s2_walk_data *data = arg;

	if (flag == KVM_PGTABLE_WALK_LEAF) {

		if ((data->size == 0) && ((*ptep) & ~ATTR_MASK) == 0) {
			return 0;
		}

		if ((data->size == 0) && ((*ptep) & ~ATTR_MASK)){
			data->size++;
			data->start_ipa = addr;
			data->start_phys = (*ptep) & ~ATTR_MASK;
			data->attr = (*ptep) & ATTR_MASK;
			data->level = level;
			return 0;
		}
		if ((data->attr == ((*ptep) & ATTR_MASK)) &&
		    (data->level == level)  &&
		    ((*ptep) & ~ATTR_MASK) == data->start_phys + data->size * (1 << bit_shift(level))) {
			data->size++;
			return 0;
		}
		print_entry(data, ptep, level, addr);

		if (((*ptep) & ~0xfff) == 0) {
			data->size = 0;
			return 0;
		}
		data->size = 1;
		data->level = level;
		data->start_ipa = addr;
		data->start_phys = (*ptep)  & ~ATTR_MASK;
		data->attr = (*ptep) & ATTR_MASK;
		data->phys =  (*ptep) & ~0xfff;

	}
	if (flag == KVM_PGTABLE_WALK_TABLE_POST) {
		if (data->size) {
			print_entry(data, ptep, level, addr);
		}
		data->size = 0;
	}

	return 0;
}

int print_mappings(u32 id, u64 addr, u64 size)
{
	struct kvm_pgtable *pgt;
	struct pkvm_hyp_vm *vm;

	struct s2_walk_data s2_data = {
			.size = 0,
	};
	struct kvm_pgtable_walker walker_s2 = {
		.cb	= print_mapping_walker,
		.flags	= KVM_PGTABLE_WALK_LEAF |
			  KVM_PGTABLE_WALK_TABLE_POST,
		.arg	= &s2_data,
	};

	if (id == 0) {
		//hypervisor mapping. TODO fix attribute printing
		pgt = &pkvm_pgtable;
	}
	else if (id == 1)
		pgt = host_mmu.arch.mmu.pgt;
	else {
		vm = get_vm_by_handle(id - 2 + 0x1000);
		if (!vm)
			return -EINVAL;
		pgt = &vm->pgt;
	}

	hyp_print("print_mappings id %x addr %llx, size %llx\n", id, addr, size);

	return kvm_pgtable_walk(pgt, addr, size, &walker_s2);
}

