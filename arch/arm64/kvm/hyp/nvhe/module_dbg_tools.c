// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022 - Google LLC
 */
#include <nvhe/pkvm.h>
#include <nvhe/spinlock.h>
#include <nvhe/mem_protect.h>
#include <nvhe/mm.h>

#include <nvhe/module_dbg_tools.h>

static int (*__hyp_vprint)(const char *format, va_list va);

static inline bool hyp_print_enabled(void)
{
	/* Paired with __pkvm_register_hyp_vprint_function()'s cmpxchg */
	return !!smp_load_acquire(&__hyp_vprint);
}

int hyp_print(const char *fmt, ...)
{
	va_list args;
	int ret = -ENODEV;

	if (hyp_print_enabled()) {
		va_start(args, fmt);
		ret = __hyp_vprint(fmt, args);
		va_end(args);
	}
	return ret;
}

int __pkvm_register_hyp_vprint_function(int (*cb)(const char *format, va_list va))
{
	int r = 0;

	/*
	 * Paired with smp_load_acquire(&__hyp_print)
	 * Ensure memory stores hapenning during a pKVM
	 * module init are observed before executing the callback.
	 */
	r = cmpxchg_release(&__hyp_vprint, NULL, cb) ? -EBUSY : 0;
	return r;
}

struct dbg_tool_ops hyp_dbg_tools_ops = {
	.pkvm_get_hyp_vm = pkvm_get_hyp_vm,
	.pkvm_put_hyp_vm = pkvm_put_hyp_vm,
	.kvm_pgtable_walk = kvm_pgtable_walk,
	.host_mmu = &host_mmu,
	.pkvm_pgtable = &pkvm_pgtable,
	.register_hyp_vprint = __pkvm_register_hyp_vprint_function,
};
