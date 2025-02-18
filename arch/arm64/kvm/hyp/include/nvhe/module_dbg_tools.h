/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef __KVM_HYP_MODULE_DBG_TOOLS
#define __KVM_HYP_MODULE_DBG_TOOLS
#include <nvhe/pkvm.h>

struct dbg_tool_ops {
	struct pkvm_hyp_vm *(*pkvm_get_hyp_vm)(pkvm_handle_t handle);
	void (*pkvm_put_hyp_vm)(struct pkvm_hyp_vm *hyp_vm);
	int (*kvm_pgtable_walk)(struct kvm_pgtable *pgt, u64 addr, u64 size,
			     struct kvm_pgtable_walker *walker);
	struct host_mmu *host_mmu;
	struct kvm_pgtable *pkvm_pgtable;
	int (*register_hyp_print)(int (*hyp_print_cb)(const char *fmt, ...));
};

extern struct dbg_tool_ops hyp_dbg_tools_ops;
#endif
