// SPDX-License-Identifier: GPL-2.0
#include <linux/string.h>
#include <linux/bitops.h>

#include <nvhe/chacha.h>
#include <asm-generic/unaligned.h>
#include <asm-generic/bug.h>

void __crypto_xor(u8 *dst, const u8 *src1, const u8 *src2, unsigned int len)
{
	int relalign = 0;

	if (!IS_ENABLED(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS)) {
		int size = sizeof(unsigned long);
		int d = (((unsigned long)dst ^ (unsigned long)src1) |
			 ((unsigned long)dst ^ (unsigned long)src2)) &
			(size - 1);

		relalign = d ? 1 << __ffs(d) : size;

		/*
		 * If we care about alignment, process as many bytes as
		 * needed to advance dst and src to values whose alignments
		 * equal their relative alignment. This will allow us to
		 * process the remainder of the input using optimal strides.
		 */
		while (((unsigned long)dst & (relalign - 1)) && len > 0) {
			*dst++ = *src1++ ^ *src2++;
			len--;
		}
	}

	while (IS_ENABLED(CONFIG_64BIT) && len >= 8 && !(relalign & 7)) {
		*(u64 *)dst = *(u64 *)src1 ^  *(u64 *)src2;
		dst += 8;
		src1 += 8;
		src2 += 8;
		len -= 8;
	}

	while (len >= 4 && !(relalign & 3)) {
		*(u32 *)dst = *(u32 *)src1 ^ *(u32 *)src2;
		dst += 4;
		src1 += 4;
		src2 += 4;
		len -= 4;
	}

	while (len >= 2 && !(relalign & 1)) {
		*(u16 *)dst = *(u16 *)src1 ^ *(u16 *)src2;
		dst += 2;
		src1 += 2;
		src2 += 2;
		len -= 2;
	}

	while (len--)
		*dst++ = *src1++ ^ *src2++;
}

static inline void crypto_xor_cpy(u8 *dst, const u8 *src1, const u8 *src2,
				  unsigned int size)
{
	if (IS_ENABLED(CONFIG_HAVE_EFFICIENT_UNALIGNED_ACCESS) &&
		__builtin_constant_p(size) &&
		(size % sizeof(unsigned long)) == 0) {
		unsigned long *d = (unsigned long *)dst;
		unsigned long *s1 = (unsigned long *)src1;
		unsigned long *s2 = (unsigned long *)src2;

		while (size > 0) {
			*d++ = *s1++ ^ *s2++;
			size -= sizeof(unsigned long);
		}
	} else {
		__crypto_xor(dst, src1, src2, size);
	}
}

static void chacha_permute(u32 *x, int nrounds)
{
	int i;

	for (i = 0; i < nrounds; i += 2) {
		x[0]  += x[4];    x[12] = rol32(x[12] ^ x[0],  16);
		x[1]  += x[5];    x[13] = rol32(x[13] ^ x[1],  16);
		x[2]  += x[6];    x[14] = rol32(x[14] ^ x[2],  16);
		x[3]  += x[7];    x[15] = rol32(x[15] ^ x[3],  16);

		x[8]  += x[12];   x[4]  = rol32(x[4]  ^ x[8],  12);
		x[9]  += x[13];   x[5]  = rol32(x[5]  ^ x[9],  12);
		x[10] += x[14];   x[6]  = rol32(x[6]  ^ x[10], 12);
		x[11] += x[15];   x[7]  = rol32(x[7]  ^ x[11], 12);

		x[0]  += x[4];    x[12] = rol32(x[12] ^ x[0],   8);
		x[1]  += x[5];    x[13] = rol32(x[13] ^ x[1],   8);
		x[2]  += x[6];    x[14] = rol32(x[14] ^ x[2],   8);
		x[3]  += x[7];    x[15] = rol32(x[15] ^ x[3],   8);

		x[8]  += x[12];   x[4]  = rol32(x[4]  ^ x[8],   7);
		x[9]  += x[13];   x[5]  = rol32(x[5]  ^ x[9],   7);
		x[10] += x[14];   x[6]  = rol32(x[6]  ^ x[10],  7);
		x[11] += x[15];   x[7]  = rol32(x[7]  ^ x[11],  7);

		x[0]  += x[5];    x[15] = rol32(x[15] ^ x[0],  16);
		x[1]  += x[6];    x[12] = rol32(x[12] ^ x[1],  16);
		x[2]  += x[7];    x[13] = rol32(x[13] ^ x[2],  16);
		x[3]  += x[4];    x[14] = rol32(x[14] ^ x[3],  16);

		x[10] += x[15];   x[5]  = rol32(x[5]  ^ x[10], 12);
		x[11] += x[12];   x[6]  = rol32(x[6]  ^ x[11], 12);
		x[8]  += x[13];   x[7]  = rol32(x[7]  ^ x[8],  12);
		x[9]  += x[14];   x[4]  = rol32(x[4]  ^ x[9],  12);

		x[0]  += x[5];    x[15] = rol32(x[15] ^ x[0],   8);
		x[1]  += x[6];    x[12] = rol32(x[12] ^ x[1],   8);
		x[2]  += x[7];    x[13] = rol32(x[13] ^ x[2],   8);
		x[3]  += x[4];    x[14] = rol32(x[14] ^ x[3],   8);

		x[10] += x[15];   x[5]  = rol32(x[5]  ^ x[10],  7);
		x[11] += x[12];   x[6]  = rol32(x[6]  ^ x[11],  7);
		x[8]  += x[13];   x[7]  = rol32(x[7]  ^ x[8],   7);
		x[9]  += x[14];   x[4]  = rol32(x[4]  ^ x[9],   7);
	}
}

/**
 * chacha_block_generic - generate one keystream block and increment block counter
 * @state: input state matrix (16 32-bit words)
 * @stream: output keystream block (64 bytes)
 * @nrounds: number of rounds (20 or 12; 20 is recommended)
 *
 * This is the ChaCha core, a function from 64-byte strings to 64-byte strings.
 * The caller has already converted the endianness of the input.  This function
 * also handles incrementing the block counter in the input matrix.
 */
void chacha_block_generic(u32 *state, u8 *stream, int nrounds)
{
	u32 x[16];
	int i;

	memcpy(x, state, 64);

	chacha_permute(x, nrounds);

	for (i = 0; i < ARRAY_SIZE(x); i++)
		put_unaligned_le32(x[i] + state[i], &stream[i * sizeof(u32)]);

	state[12]++;
}

void chacha_crypt_generic(u32 *state, u8 *dst, const u8 *src,
			  unsigned int bytes, int nrounds)
{
	/* aligned to potentially speed up crypto_xor() */
	u8 stream[CHACHA_BLOCK_SIZE] __aligned(sizeof(long));

	while (bytes >= CHACHA_BLOCK_SIZE) {
		chacha_block_generic(state, stream, nrounds);
		crypto_xor_cpy(dst, src, stream, CHACHA_BLOCK_SIZE);
		bytes -= CHACHA_BLOCK_SIZE;
		dst += CHACHA_BLOCK_SIZE;
		src += CHACHA_BLOCK_SIZE;
	}
	if (bytes) {
		chacha_block_generic(state, stream, nrounds);
		crypto_xor_cpy(dst, src, stream, bytes);
	}
}
