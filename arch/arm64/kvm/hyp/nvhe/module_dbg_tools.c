// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022 - Google LLC
 */
#include <nvhe/pkvm.h>
#include <nvhe/spinlock.h>

static int (*__hyp_print)(const char *fmt, ...);

static inline bool hyp_print_enabled(void)
{
	/* Paired with __pkvm_register_hyp_print_function()'s cmpxchg */
	return !!smp_load_acquire(&__hyp_print);
}

int hyp_print(const char *fmt, ...)
{
	va_list args;
	int ret = -ENODEV;

	if (hyp_print_enabled()) {
		va_start(args, fmt);
		ret = __hyp_print(fmt, args);
		va_end(args);
	}
	return ret;
}


int __pkvm_register_hyp_print_function(int (*cb)(const char *fmt, ...))
{
	int r = 0;
	//hyp_print("cb %llx\n",cb);
	/*
	 * Paired with smp_load_acquire(&__hyp_print)
	 * Ensure memory stores hapenning during a pKVM
	 * module init are observed before executing the callback.
	 */
	r = cmpxchg_release(&__hyp_print, NULL, cb) ? -EBUSY : 0;
	hyp_print("register_hyp_print_function\n");
	return r;
}
