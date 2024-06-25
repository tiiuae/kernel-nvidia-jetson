// SPDX-License-Identifier: GPL-2.0
#include <nvhe/ramlog.h>
#include <hyp/hyp_debug.h>

#ifdef CONFIG_KVM_ARM_HYP_DEBUG_RAMLOG

/* Tiny ram log */
char __rlog[RAMLOG_SIZE];
int __rp;

static inline void unvalid_log_chacha(void);
static inline bool is_log_chacha_initialized(void);

/* keys should to be gotten from keystorage when it will be implemented */
u32 chacha_state[16] = {0};
u32 chacha_key[8] = {
	0x09080706, 0x10203040, 0x05060708, 0x50607080,
	0xa9a8a7a6, 0x1a2a3a4a, 0xa5a6a7a8, 0x5a6a7a8a
};
u8 chacha_iv[16] = {
	0x10, 0x0f, 0x02, 0xe0,
	0x30, 0x0d, 0x04, 0xc0,
	0x50, 0x0b, 0x06, 0xa0,
	0x70, 0x09, 0x08, 0x80,
};

struct hyp_timestamp hts = {0};

static inline u64 getcntpct_el0(void)
{
	u64 res;

	isb();
	asm volatile("mrs %0, cntpct_el0" : "=r" (res) :: "memory");
	return res;
}

static inline u64 getcntfrq_el0(void)
{
	u64 res;

	isb();
	asm volatile("mrs %0, cntfrq_el0" : "=r" (res) :: "memory");
	return res;
}

/* poor implementation of the timer from cntpct_el0
 * could be not accurate in lowest orders
 */
inline void gettimestamp(struct hyp_timestamp *hts)
{
	int i;
	u64 nsec = 0, rem = 0;
	u64 clks = getcntpct_el0();
	u64 freq = getcntfrq_el0();

	hts->sec = clks/freq;
	rem = clks % freq;
	for (i = 0; i < 10; i++) {
		freq /= 10;
		nsec += rem / freq;
		rem = rem % freq;
		nsec *= 10;
	}
	hts->nsec = nsec / 10;
}

inline char *rlogp_head(void)
{
	return __rlog;
}

/* returns a pointer to 64-byte entry
 */
inline char *rlogp_entry(int entry)
{
	return &__rlog[entry * LOG_ENTRY_LENGTH];
}

inline int rlog_cur_entry(void)
{
	return __rp / LOG_ENTRY_LENGTH;
}

static void __decrypt_log_data(void)
{
	 /* temporarily initial chacha state written
	  * to the first entry of the log, use it for decrytion and skip
	  * during decryption itself
	  */
	int log_len = rlog_cur_entry() * LOG_ENTRY_LENGTH;

	 chacha_crypt_generic((u32 *) rlogp_head(),
					      rlogp_entry(1),
					      rlogp_entry(1),
					      log_len,
					      20);
	 unvalid_log_chacha();
}

static void __encrypt_log_data(void)
{
	if (__rp > LOG_ENTRY_LENGTH && is_log_chacha_initialized()) {
		chacha_crypt_generic(chacha_state,
							rlogp_entry(1),
							rlogp_entry(1),
							rlog_cur_entry() * LOG_ENTRY_LENGTH,
							20);
	}
}

/* generate chacha init state and encrypt log data of there is presence of it */
static inline void log_chacha_init(void)
{
	chacha_init_generic(chacha_state, chacha_key, chacha_iv);
	/* put whole chacha's init state into the first log entry */
	/* TODO: crypt it with ECDH shared secret */
	memcpy(rlogp_head(), chacha_state, 64);
	if (__rp < LOG_ENTRY_LENGTH)
		__rp = LOG_ENTRY_LENGTH;
	else
		__encrypt_log_data();
}

static inline bool is_log_chacha_initialized(void)
{
	/* TODO: find more proper way checking valid chacha state */
	if (chacha_state[0] == 0 && chacha_state[1] == 0)
		return false;
	return true;
}

static inline void log_chacha_check_and_init(void)
{
	if (!is_log_chacha_initialized())
		log_chacha_init();
}

static inline void unvalid_log_chacha(void)
{
	chacha_state[0] = 0;
	chacha_state[1] = 0;
}

/* fill log entry with padding to make it 64byte multiple sized */
static inline void __align_log_entry(void)
{
	if (__rlog[__rp - 1] == '\n')
		__rp--;
	for (; __rp % LOG_ENTRY_LENGTH != 0; __rp++)
		__rlog[__rp] = ' ';
	__rlog[__rp - 2] = '.';
	__rlog[__rp - 1] = '\0';
}


void hyp_ramlog(const char *fmt, ...)
{
	va_list args;
	int count = 0;
	unsigned int head_to_crypt, written_entries;

	/* if log array contains less than 2*LOG_ENTRY_LENGTH - reinit log
	 */
	if ((__rp + 2 * LOG_ENTRY_LENGTH) >= RAMLOG_SIZE) {
		unvalid_log_chacha();
		__rp = 0;
	}

	log_chacha_check_and_init();

	va_start(args, fmt);
	count = hyp_vsnprintf(&__rlog[__rp], 2 * LOG_ENTRY_LENGTH, fmt, args);
	va_end(args);

	__rp += count;

	/* align entry to be multiple of LOG_ENTRY_LENGTH
	 * __rp will be moved to be multiple of 64
	 */
	__align_log_entry();

	/* crypt new log entry(ies) in place
	 */
	written_entries = count / LOG_ENTRY_LENGTH + 1;
	head_to_crypt = rlog_cur_entry() - written_entries;
	chacha_crypt_generic(chacha_state,
					     rlogp_entry(head_to_crypt),
					     rlogp_entry(head_to_crypt),
					     written_entries * LOG_ENTRY_LENGTH,
					     20);
}

/**
 * The function decrypt, print, encrypt back the ramlog
 */
void print_rlog(void)
{
	int i = rlog_cur_entry();

	__decrypt_log_data();
	for (; i > 1; i--)
		hyp_print("%s\n", rlogp_entry(rlog_cur_entry() - i + 1));

	/* after decryption current chacha state is invalid. reinit chacha
	 * if there is data it will be encrypted again
	 */
	log_chacha_init();
}

void output_rlog(u64 dump)
{
	if (dump) {
		int cur = rlog_cur_entry();

		memcpy(dbg_buffer->data, rlogp_head(), LOG_ENTRY_LENGTH * cur);
		dbg_buffer->datalen += LOG_ENTRY_LENGTH * cur;
	} else {
		print_rlog();
	}
}

#endif /* CONFIG_KVM_ARM_HYP_DEBUG_RAMLOG */
