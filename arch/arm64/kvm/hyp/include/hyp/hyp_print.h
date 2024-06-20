/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef __ARM64_KVM_HYP_HYP_PRINT_H__
#define __ARM64_KVM_HYP_HYP_PRINT_H__

int hyp_print(const char *fmt, ...);
int hyp_vsnprintf(char *str, size_t size, const char *format, va_list ap);
int hyp_snprint(char *s, size_t slen, const char *format, ...);
int hyp_dbg_print(const char *fmt, ...);

#endif
