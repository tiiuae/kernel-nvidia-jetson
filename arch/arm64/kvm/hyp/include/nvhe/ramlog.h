/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __NVHE_RAMLOG_H
#define __NVHE_RAMLOG_H

#ifdef CONFIG_KVM_ARM_HYP_DEBUG_RAMLOG
#include <linux/kernel.h>
#include <linux/string.h>
#include <asm/page-def.h>
#include <hyp/hyp_print.h>
#include <nvhe/chacha.h>

#define LOG_ENTRY_LENGTH 64
#define RAMLOG_SIZE PAGE_SIZE

#define __hyp_read_reg(r)                                    \
	__extension__({                                        \
		uint64_t value;                                    \
		__asm__ __volatile__("mrs	%0, " #r               \
					 : "=r"(value));                       \
		value;                                             \
	})

#ifdef CONFIG_KVM_ARM_HYP_DEBUG_RAMLOG_DIRECT_PRINT
# define hyp_ramlog_ts(fmt, ...) do { \
		gettimestamp(&hts);         \
		hyp_print("[rl %d.%ld] " fmt, hts.sec, hts.nsec, __VA_ARGS__);  \
		hyp_ramlog("[rl %d.%ld] " fmt, hts.sec, hts.nsec, __VA_ARGS__);  \
} while (0)

#else // CONFIG_KVM_ARM_HYP_DEBUG_RAMLOG_DIRECT_PRINT

# define hyp_ramlog_ts(fmt, ...) do { \
		gettimestamp(&hts);         \
		hyp_ramlog("[rl %d.%ld] " fmt, hts.sec, hts.nsec, __VA_ARGS__);  \
} while (0)
#endif // CONFIG_KVM_ARM_HYP_DEBUG_RAMLOG_DIRECT_PRINT

#define hyp_ramlog_reg(reg) \
		hyp_ramlog_ts(#reg "\t- %016llx\n", __hyp_read_reg(reg))

struct hyp_timestamp {
	u64 sec;
	u64 nsec;
};

extern int hyp_vsnprintf(char *a, size_t b, const char *c, va_list d);
extern struct hyp_timestamp hts;

inline void gettimestamp(struct hyp_timestamp *);

inline char *rlogp_head(void);

inline char *rlogp_entry(int entry);

inline int rlog_cur_entry(void);

void hyp_ramlog(const char *fmt, ...);

/**
 * The function decrypts, prints out and encrypts the ramlog back
 */
void print_rlog(void);

/**
 * The function being called by a hvc __hyp_dbg() call handler. Able to
 * just call print_rlog or copy an encrypted ramlog to shared with the host
 * buffer
 *
 * @dump defines will ramlog being printed or copied to the host for to dump
 */
void output_rlog(u64 dump);

#else /* CONFIG_KVM_ARM_HYP_DEBUG_RAMLOG */
#define hyp_ramlog_ts(...)
#define hyp_ramlog_reg(reg)
inline void  gettimestamp(struct hyp_timestamp *) {}
inline char *rlogp_head(void) { return ""; }
inline char *rlogp_entry(int entry) { return NULL; }
inline int   rlog_cur_entry(void) { return -1; }
void hyp_ramlog(const char *fmt, ...) {}
void print_rlog(void) {}
void output_rlog(u64) {}
#endif /* CONFIG_KVM_ARM_HYP_DEBUG_RAMLOG */
#endif /* __NVHE_RAMLOG_H */
