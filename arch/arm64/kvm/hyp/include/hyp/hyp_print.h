/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __ARM64_KVM_HYP_HYP_PRINT_H__
#define __ARM64_KVM_HYP_HYP_PRINT_H__

int hyp_print(const char *fmt, ...);
int hyp_vsnprintf(char *str, size_t size, const char *format, va_list ap);

#endif
