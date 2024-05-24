// SPDX-License-Identifier: GPL-2.0-only

#ifndef __ARM64_KVM_HYP_HYP_PRINT_H__
#define __ARM64_KVM_HYP_HYP_PRINT_H__

struct dgb_buf {
	u64 size;
	u64 datalen;
	u8 data[];
};
extern struct dgb_buf *dbg_buffer;

int hyp_print(const char *fmt, ...);
int hyp_snprint(char *s, size_t slen, const char *format, ...);
int hyp_dbg_print(const char *fmt, ...);

#endif
