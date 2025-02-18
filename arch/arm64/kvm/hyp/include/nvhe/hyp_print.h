/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __ARM64_KVM_HYP_HYP_PRINT_H__
#define __ARM64_KVM_HYP_HYP_PRINT_H__
#include <linux/kconfig.h>
#ifdef CONFIG_PKVM_VENDOR_MODULE_OPS
int hyp_print(const char *fmt, ...);
int hyp_dbg_print(const char *fmt, ...);
#else
static inline int hyp_print(const char *fmt, ...) { return 0; }
static inline int hyp_dbg_print(const char *fmt, ...) { return 0; };
#endif

#endif
