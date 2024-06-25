/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KVM_ARM_NVHE_CHACHA_H
#define _KVM_ARM_NVHE_CHACHA_H

#define CHACHA_KEY_SIZE		32
#define CHACHA_BLOCK_SIZE	64

#define CHACHA_STATE_WORDS	(CHACHA_BLOCK_SIZE / sizeof(u32))

#include <asm/unaligned.h>

void chacha_crypt_generic(u32 *state, u8 *dst, const u8 *src,
			  unsigned int bytes, int nrounds);

enum chacha_constants { /* expand 32-byte k */
	CHACHA_CONSTANT_EXPA = 0x61707865U,
	CHACHA_CONSTANT_ND_3 = 0x3320646eU,
	CHACHA_CONSTANT_2_BY = 0x79622d32U,
	CHACHA_CONSTANT_TE_K = 0x6b206574U
};

static inline void chacha_init_consts(u32 *state)
{
	state[0]  = CHACHA_CONSTANT_EXPA;
	state[1]  = CHACHA_CONSTANT_ND_3;
	state[2]  = CHACHA_CONSTANT_2_BY;
	state[3]  = CHACHA_CONSTANT_TE_K;
}

static inline void chacha_init_generic(u32 *state, const u32 *key, const u8 *iv)
{
	chacha_init_consts(state);
	state[4]  = key[0];
	state[5]  = key[1];
	state[6]  = key[2];
	state[7]  = key[3];
	state[8]  = key[4];
	state[9]  = key[5];
	state[10] = key[6];
	state[11] = key[7];
	state[12] = get_unaligned_le32(iv +  0);
	state[13] = get_unaligned_le32(iv +  4);
	state[14] = get_unaligned_le32(iv +  8);
	state[15] = get_unaligned_le32(iv + 12);
}

#endif //_KVM_ARM_NVHE_CHACHA_H
