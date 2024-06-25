/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KVM_ARM64_DUMP_REGS_H
#define _KVM_ARM64_DUMP_REGS_H

#include <linux/kernel.h>
#include <nvhe/ramlog.h>

#ifdef CONFIG_KVM_ARM_HYP_DEBUG_RAMLOG
void debug_dump_csrs(void);
#else
void debug_dump_csrs(void) { }
#endif

#endif /* _KVM_ARM64_DUMP_REGS_H */
