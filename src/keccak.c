/* $Id: keccak.c 259 2011-07-19 22:11:27Z tp $ */
/*
 * Keccak implementation.
 *
 * ==========================(LICENSE BEGIN)============================
 *
 * Copyright (c) 2007-2010  Projet RNRT SAPHIR
 * 
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * ===========================(LICENSE END)=============================
 *
 * @author   Thomas Pornin <thomas.pornin@cryptolog.com>
 */

#include <stddef.h>
#include <string.h>

#include "sph_keccak.h"

#ifdef __cplusplus
extern "C"{
#endif

/*
 * Parameters:
 *
 *  SPH_KECCAK_64          use a 64-bit type
 *  SPH_KECCAK_UNROLL      number of loops to unroll (0/undef for full unroll)
 *  SPH_KECCAK_INTERLEAVE  use bit-interleaving (32-bit type only)
 *  SPH_KECCAK_NOCOPY      do not copy the state into local variables
 * 
 * If there is no usable 64-bit type, the code automatically switches
 * back to the 32-bit implementation.
 *
 * Some tests on an Intel Core2 Q6600 (both 64-bit and 32-bit, 32 kB L1
 * code cache), a PowerPC (G3, 32 kB L1 code cache), an ARM920T core
 * (16 kB L1 code cache), and a small MIPS-compatible CPU (Broadcom BCM3302,
 * 8 kB L1 code cache), seem to show that the following are optimal:
 *
 * -- x86, 64-bit: use the 64-bit implementation, unroll 8 rounds,
 * do not copy the state; unrolling 2, 6 or all rounds also provides
 * near-optimal performance.
 * -- x86, 32-bit: use the 32-bit implementation, unroll 6 rounds,
 * interleave, do not copy the state. Unrolling 1, 2, 4 or 8 rounds
 * also provides near-optimal performance.
 * -- PowerPC: use the 64-bit implementation, unroll 8 rounds,
 * copy the state. Unrolling 4 or 6 rounds is near-optimal.
 * -- ARM: use the 64-bit implementation, unroll 2 or 4 rounds,
 * copy the state.
 * -- MIPS: use the 64-bit implementation, unroll 2 rounds, copy
 * the state. Unrolling only 1 round is also near-optimal.
 *
 * Also, interleaving does not always yield actual improvements when
 * using a 32-bit implementation; in particular when the architecture
 * does not offer a native rotation opcode (interleaving replaces one
 * 64-bit rotation with two 32-bit rotations, which is a gain only if
 * there is a native 32-bit rotation opcode and not a native 64-bit
 * rotation opcode; also, interleaving implies a small overhead when
 * processing input words).
 *
 * To sum up:
 * -- when possible, use the 64-bit code
 * -- exception: on 32-bit x86, use 32-bit code
 * -- when using 32-bit code, use interleaving
 * -- copy the state, except on x86
 * -- unroll 8 rounds on "big" machine, 2 rounds on "small" machines
 */

#if SPH_SMALL_FOOTPRINT && !defined SPH_SMALL_FOOTPRINT_KECCAK
#define SPH_SMALL_FOOTPRINT_KECCAK   1
#endif

/*
 * By default, we select the 64-bit implementation if a 64-bit type
 * is available, unless a 32-bit x86 is detected.
 */
#if !defined SPH_KECCAK_64 && SPH_64 \
	&& !(defined __i386__ || SPH_I386_GCC || SPH_I386_MSVC)
#define SPH_KECCAK_64   1
#endif

/*
 * If using a 32-bit implementation, we prefer to interleave.
 */
#if !SPH_KECCAK_64 && !defined SPH_KECCAK_INTERLEAVE
#define SPH_KECCAK_INTERLEAVE   1
#endif

/*
 * Unroll 8 rounds on big systems, 2 rounds on small systems.
 */
#ifndef SPH_KECCAK_UNROLL
#if SPH_SMALL_FOOTPRINT_KECCAK
#define SPH_KECCAK_UNROLL   2
#else
#define SPH_KECCAK_UNROLL   8
#endif
#endif

/*
 * We do not want to copy the state to local variables on x86 (32-bit
 * and 64-bit alike).
 */
#ifndef SPH_KECCAK_NOCOPY
#if defined __i386__ || defined __x86_64 || SPH_I386_MSVC || SPH_I386_GCC
#define SPH_KECCAK_NOCOPY   1
#else
#define SPH_KECCAK_NOCOPY   0
#endif
#endif

#ifdef _MSC_VER
#pragma warning (disable: 4146)
#endif

#if SPH_KECCAK_64

static const sph_u64 RC[] = {
	SPH_C64(0x0000000000000001), SPH_C64(0x0000000000008082),
	SPH_C64(0x800000000000808A), SPH_C64(0x8000000080008000),
	SPH_C64(0x000000000000808B), SPH_C64(0x0000000080000001),
	SPH_C64(0x8000000080008081), SPH_C64(0x8000000000008009),
	SPH_C64(0x000000000000008A), SPH_C64(0x0000000000000088),
	SPH_C64(0x0000000080008009), SPH_C64(0x000000008000000A),
	SPH_C64(0x000000008000808B), SPH_C64(0x800000000000008B),
	SPH_C64(0x8000000000008089), SPH_C64(0x8000000000008003),
	SPH_C64(0x8000000000008002), SPH_C64(0x8000000000000080),
	SPH_C64(0x000000000000800A), SPH_C64(0x800000008000000A),
	SPH_C64(0x8000000080008081), SPH_C64(0x8000000000008080),
	SPH_C64(0x0000000080000001), SPH_C64(0x8000000080008008)
};

#if SPH_KECCAK_NOCOPY

#define a00   (kc->u.wide[ 0])
#define a10   (kc->u.wide[ 1])
#define a20   (kc->u.wide[ 2])
#define a30   (kc->u.wide[ 3])
#define a40   (kc->u.wide[ 4])
#define a01   (kc->u.wide[ 5])
#define a11   (kc->u.wide[ 6])
#define a21   (kc->u.wide[ 7])
#define a31   (kc->u.wide[ 8])
#define a41   (kc->u.wide[ 9])
#define a02   (kc->u.wide[10])
#define a12   (kc->u.wide[11])
#define a22   (kc->u.wide[12])
#define a32   (kc->u.wide[13])
#define a42   (kc->u.wide[14])
#define a03   (kc->u.wide[15])
#define a13   (kc->u.wide[16])
#define a23   (kc->u.wide[17])
#define a33   (kc->u.wide[18])
#define a43   (kc->u.wide[19])
#define a04   (kc->u.wide[20])
#define a14   (kc->u.wide[21])
#define a24   (kc->u.wide[22])
#define a34   (kc->u.wide[23])
#define a44   (kc->u.wide[24])

#define DECL_STATE
#define READ_STATE(sc)
#define WRITE_STATE(sc)

#define INPUT_BUF(size)   do { \
		size_t j; \
		for (j = 0; j < (size); j += 8) { \
			kc->u.wide[j >> 3] ^= sph_dec64le_aligned(buf + j); \
		} \
	} while (0)

#define INPUT_BUF144   INPUT_BUF(144)
#define INPUT_BUF136   INPUT_BUF(136)
#define INPUT_BUF104   INPUT_BUF(104)
#define INPUT_BUF72    INPUT_BUF(72)

#else

#define DECL_STATE \
	sph_u64 a00, a01, a02, a03, a04; \
	sph_u64 a10, a11, a12, a13, a14; \
	sph_u64 a20, a21, a22, a23, a24; \
	sph_u64 a30, a31, a32, a33, a34; \
	sph_u64 a40, a41, a42, a43, a44;

#define READ_STATE(state)   do { \
		a00 = (state)->u.wide[ 0]; \
		a10 = (state)->u.wide[ 1]; \
		a20 = (state)->u.wide[ 2]; \
		a30 = (state)->u.wide[ 3]; \
		a40 = (state)->u.wide[ 4]; \
		a01 = (state)->u.wide[ 5]; \
		a11 = (state)->u.wide[ 6]; \
		a21 = (state)->u.wide[ 7]; \
		a31 = (state)->u.wide[ 8]; \
		a41 = (state)->u.wide[ 9]; \
		a02 = (state)->u.wide[10]; \
		a12 = (state)->u.wide[11]; \
		a22 = (state)->u.wide[12]; \
		a32 = (state)->u.wide[13]; \
		a42 = (state)->u.wide[14]; \
		a03 = (state)->u.wide[15]; \
		a13 = (state)->u.wide[16]; \
		a23 = (state)->u.wide[17]; \
		a33 = (state)->u.wide[18]; \
		a43 = (state)->u.wide[19]; \
		a04 = (state)->u.wide[20]; \
		a14 = (state)->u.wide[21]; \
		a24 = (state)->u.wide[22]; \
		a34 = (state)->u.wide[23]; \
		a44 = (state)->u.wide[24]; \
	} while (0)

#define WRITE_STATE(state)   do { \
		(state)->u.wide[ 0] = a00; \
		(state)->u.wide[ 1] = a10; \
		(state)->u.wide[ 2] = a20; \
		(state)->u.wide[ 3] = a30; \
		(state)->u.wide[ 4] = a40; \
		(state)->u.wide[ 5] = a01; \
		(state)->u.wide[ 6] = a11; \
		(state)->u.wide[ 7] = a21; \
		(state)->u.wide[ 8] = a31; \
		(state)->u.wide[ 9] = a41; \
		(state)->u.wide[10] = a02; \
		(state)->u.wide[11] = a12; \
		(state)->u.wide[12] = a22; \
		(state)->u.wide[13] = a32; \
		(state)->u.wide[14] = a42; \
		(state)->u.wide[15] = a03; \
		(state)->u.wide[16] = a13; \
		(state)->u.wide[17] = a23; \
		(state)->u.wide[18] = a33; \
		(state)->u.wide[19] = a43; \
		(state)->u.wide[20] = a04; \
		(state)->u.wide[21] = a14; \
		(state)->u.wide[22] = a24; \
		(state)->u.wide[23] = a34; \
		(state)->u.wide[24] = a44; \
	} while (0)

#define INPUT_BUF144   do { \
		a00 ^= sph_dec64le_aligned(buf +   0); \
		a10 ^= sph_dec64le_aligned(buf +   8); \
		a20 ^= sph_dec64le_aligned(buf +  16); \
		a30 ^= sph_dec64le_aligned(buf +  24); \
		a40 ^= sph_dec64le_aligned(buf +  32); \
		a01 ^= sph_dec64le_aligned(buf +  40); \
		a11 ^= sph_dec64le_aligned(buf +  48); \
		a21 ^= sph_dec64le_aligned(buf +  56); \
		a31 ^= sph_dec64le_aligned(buf +  64); \
		a41 ^= sph_dec64le_aligned(buf +  72); \
		a02 ^= sph_dec64le_aligned(buf +  80); \
		a12 ^= sph_dec64le_aligned(buf +  88); \
		a22 ^= sph_dec64le_aligned(buf +  96); \
		a32 ^= sph_dec64le_aligned(buf + 104); \
		a42 ^= sph_dec64le_aligned(buf + 112); \
		a03 ^= sph_dec64le_aligned(buf + 120); \
		a13 ^= sph_dec64le_aligned(buf + 128); \
		a23 ^= sph_dec64le_aligned(buf + 136); \
	} while (0)

#define INPUT_BUF136   do { \
		a00 ^= sph_dec64le_aligned(buf +   0); \
		a10 ^= sph_dec64le_aligned(buf +   8); \
		a20 ^= sph_dec64le_aligned(buf +  16); \
		a30 ^= sph_dec64le_aligned(buf +  24); \
		a40 ^= sph_dec64le_aligned(buf +  32); \
		a01 ^= sph_dec64le_aligned(buf +  40); \
		a11 ^= sph_dec64le_aligned(buf +  48); \
		a21 ^= sph_dec64le_aligned(buf +  56); \
		a31 ^= sph_dec64le_aligned(buf +  64); \
		a41 ^= sph_dec64le_aligned(buf +  72); \
		a02 ^= sph_dec64le_aligned(buf +  80); \
		a12 ^= sph_dec64le_aligned(buf +  88); \
		a22 ^= sph_dec64le_aligned(buf +  96); \
		a32 ^= sph_dec64le_aligned(buf + 104); \
		a42 ^= sph_dec64le_aligned(buf + 112); \
		a03 ^= sph_dec64le_aligned(buf + 120); \
		a13 ^= sph_dec64le_aligned(buf + 128); \
	} while (0)

#define INPUT_BUF104   do { \
		a00 ^= sph_dec64le_aligned(buf +   0); \
		a10 ^= sph_dec64le_aligned(buf +   8); \
		a20 ^= sph_dec64le_aligned(buf +  16); \
		a30 ^= sph_dec64le_aligned(buf +  24); \
		a40 ^= sph_dec64le_aligned(buf +  32); \
		a01 ^= sph_dec64le_aligned(buf +  40); \
		a11 ^= sph_dec64le_aligned(buf +  48); \
		a21 ^= sph_dec64le_aligned(buf +  56); \
		a31 ^= sph_dec64le_aligned(buf +  64); \
		a41 ^= sph_dec64le_aligned(buf +  72); \
		a02 ^= sph_dec64le_aligned(buf +  80); \
		a12 ^= sph_dec64le_aligned(buf +  88); \
		a22 ^= sph_dec64le_aligned(buf +  96); \
	} while (0)

#define INPUT_BUF72   do { \
		a00 ^= sph_dec64le_aligned(buf +   0); \
		a10 ^= sph_dec64le_aligned(buf +   8); \
		a20 ^= sph_dec64le_aligned(buf +  16); \
		a30 ^= sph_dec64le_aligned(buf +  24); \
		a40 ^= sph_dec64le_aligned(buf +  32); \
		a01 ^= sph_dec64le_aligned(buf +  40); \
		a11 ^= sph_dec64le_aligned(buf +  48); \
		a21 ^= sph_dec64le_aligned(buf +  56); \
		a31 ^= sph_dec64le_aligned(buf +  64); \
	} while (0)

#define INPUT_BUF(lim)   do { \
		a00 ^= sph_dec64le_aligned(buf +   0); \
		a10 ^= sph_dec64le_aligned(buf +   8); \
		a20 ^= sph_dec64le_aligned(buf +  16); \
		a30 ^= sph_dec64le_aligned(buf +  24); \
		a40 ^= sph_dec64le_aligned(buf +  32); \
		a01 ^= sph_dec64le_aligned(buf +  40); \
		a11 ^= sph_dec64le_aligned(buf +  48); \
		a21 ^= sph_dec64le_aligned(buf +  56); \
		a31 ^= sph_dec64le_aligned(buf +  64); \
		if ((lim) == 72) \
			break; \
		a41 ^= sph_dec64le_aligned(buf +  72); \
		a02 ^= sph_dec64le_aligned(buf +  80); \
		a12 ^= sph_dec64le_aligned(buf +  88); \
		a22 ^= sph_dec64le_aligned(buf +  96); \
		if ((lim) == 104) \
			break; \
		a32 ^= sph_dec64le_aligned(buf + 104); \
		a42 ^= sph_dec64le_aligned(buf + 112); \
		a03 ^= sph_dec64le_aligned(buf + 120); \
		a13 ^= sph_dec64le_aligned(buf + 128); \
		if ((lim) == 136) \
			break; \
		a23 ^= sph_dec64le_aligned(buf + 136); \
	} while (0)

#endif

#define DECL64(x)        sph_u64 x
#define MOV64(d, s)      (d = s)
#define XOR64(d, a, b)   (d = a ^ b)
#define AND64(d, a, b)   (d = a & b)
#define OR64(d, a, b)    (d = a | b)
#define NOT64(d, s)      (d = SPH_T64(~s))
#define ROL64(d, v, n)   (d = SPH_ROTL64(v, n))
#define XOR64_IOTA       XOR64

#else

static const struct {
	sph_u32 high, low;
} RC[] = {
#if SPH_KECCAK_INTERLEAVE
	{ SPH_C32(0x00000000), SPH_C32(0x00000001) },
	{ SPH_C32(0x00000089), SPH_C32(0x00000000) },
	{ SPH_C32(0x8000008B), SPH_C32(0x00000000) },
	{ SPH_C32(0x80008080), SPH_C32(0x00000000) },
	{ SPH_C32(0x0000008B), SPH_C32(0x00000001) },
	{ SPH_C32(0x00008000), SPH_C32(0x00000001) },
	{ SPH_C32(0x80008088), SPH_C32(0x00000001) },
	{ SPH_C32(0x80000082), SPH_C32(0x00000001) },
	{ SPH_C32(0x0000000B), SPH_C32(0x00000000) },
	{ SPH_C32(0x0000000A), SPH_C32(0x00000000) },
	{ SPH_C32(0x00008082), SPH_C32(0x00000001) },
	{ SPH_C32(0x00008003), SPH_C32(0x00000000) },
	{ SPH_C32(0x0000808B), SPH_C32(0x00000001) },
	{ SPH_C32(0x8000000B), SPH_C32(0x00000001) },
	{ SPH_C32(0x8000008A), SPH_C32(0x00000001) },
	{ SPH_C32(0x80000081), SPH_C32(0x00000001) },
	{ SPH_C32(0x80000081), SPH_C32(0x00000000) },
	{ SPH_C32(0x80000008), SPH_C32(0x00000000) },
	{ SPH_C32(0x00000083), SPH_C32(0x00000000) },
	{ SPH_C32(0x80008003), SPH_C32(0x00000000) },
	{ SPH_C32(0x80008088), SPH_C32(0x00000001) },
	{ SPH_C32(0x80000088), SPH_C32(0x00000000) },
	{ SPH_C32(0x00008000), SPH_C32(0x00000001) },
	{ SPH_C32(0x80008082), SPH_C32(0x00000000) }
#else
	{ SPH_C32(0x00000000), SPH_C32(0x00000001) },
	{ SPH_C32(0x00000000), SPH_C32(0x00008082) },
	{ SPH_C32(0x80000000), SPH_C32(0x0000808A) },
	{ SPH_C32(0x80000000), SPH_C32(0x80008000) },
	{ SPH_C32(0x00000000), SPH_C32(0x0000808B) },
	{ SPH_C32(0x00000000), SPH_C32(0x80000001) },
	{ SPH_C32(0x80000000), SPH_C32(0x80008081) },
	{ SPH_C32(0x80000000), SPH_C32(0x00008009) },
	{ SPH_C32(0x00000000), SPH_C32(0x0000008A) },
	{ SPH_C32(0x00000000), SPH_C32(0x00000088) },
	{ SPH_C32(0x00000000), SPH_C32(0x80008009) },
	{ SPH_C32(0x00000000), SPH_C32(0x8000000A) },
	{ SPH_C32(0x00000000), SPH_C32(0x8000808B) },
	{ SPH_C32(0x80000000), SPH_C32(0x0000008B) },
	{ SPH_C32(0x80000000), SPH_C32(0x00008089) },
	{ SPH_C32(0x80000000), SPH_C32(0x00008003) },
	{ SPH_C32(0x80000000), SPH_C32(0x00008002) },
	{ SPH_C32(0x80000000), SPH_C32(0x00000080) },
	{ SPH_C32(0x00000000), SPH_C32(0x0000800A) },
	{ SPH_C32(0x80000000), SPH_C32(0x8000000A) },
	{ SPH_C32(0x80000000), SPH_C32(0x80008081) },
	{ SPH_C32(0x80000000), SPH_C32(0x00008080) },
	{ SPH_C32(0x00000000), SPH_C32(0x80000001) },
	{ SPH_C32(0x80000000), SPH_C32(0x80008008) }
#endif
};

#if SPH_KECCAK_INTERLEAVE

#define INTERLEAVE(xl, xh)   do { \
		sph_u32 l, h, t; \
		l = (xl); h = (xh); \
		t = (l ^ (l >> 1)) & SPH_C32(0x22222222); l ^= t ^ (t << 1); \
		t = (h ^ (h >> 1)) & SPH_C32(0x22222222); h ^= t ^ (t << 1); \
		t = (l ^ (l >> 2)) & SPH_C32(0x0C0C0C0C); l ^= t ^ (t << 2); \
		t = (h ^ (h >> 2)) & SPH_C32(0x0C0C0C0C); h ^= t ^ (t << 2); \
		t = (l ^ (l >> 4)) & SPH_C32(0x00F000F0); l ^= t ^ (t << 4); \
		t = (h ^ (h >> 4)) & SPH_C32(0x00F000F0); h ^= t ^ (t << 4); \
		t = (l ^ (l >> 8)) & SPH_C32(0x0000FF00); l ^= t ^ (t << 8); \
		t = (h ^ (h >> 8)) & SPH_C32(0x0000FF00); h ^= t ^ (t << 8); \
		t = (l ^ SPH_T32(h << 16)) & SPH_C32(0xFFFF0000); \
		l ^= t; h ^= t >> 16; \
		(xl) = l; (xh) = h; \
	} while (0)

#define UNINTERLEAVE(xl, xh)   do { \
		sph_u32 l, h, t; \
		l = (xl); h = (xh); \
		t = (l ^ SPH_T32(h << 16)) & SPH_C32(0xFFFF0000); \
		l ^= t; h ^= t >> 16; \
		t = (l ^ (l >> 8)) & SPH_C32(0x0000FF00); l ^= t ^ (t << 8); \
		t = (h ^ (h >> 8)) & SPH_C32(0x0000FF00); h ^= t ^ (t << 8); \
		t = (l ^ (l >> 4)) & SPH_C32(0x00F000F0); l ^= t ^ (t << 4); \
		t = (h ^ (h >> 4)) & SPH_C32(0x00F000F0); h ^= t ^ (t << 4); \
		t = (l ^ (l >> 2)) & SPH_C32(0x0C0C0C0C); l ^= t ^ (t << 2); \
		t = (h ^ (h >> 2)) & SPH_C32(0x0C0C0C0C); h ^= t ^ (t << 2); \
		t = (l ^ (l >> 1)) & SPH_C32(0x22222222); l ^= t ^ (t << 1); \
		t = (h ^ (h >> 1)) & SPH_C32(0x22222222); h ^= t ^ (t << 1); \
		(xl) = l; (xh) = h; \
	} while (0)

#else

#define INTERLEAVE(l, h)
#define UNINTERLEAVE(l, h)

#endif

#if SPH_KECCAK_NOCOPY

#define a00l   (kc->u.narrow[2 *  0 + 0])
#define a00h   (kc->u.narrow[2 *  0 + 1])
#define a10l   (kc->u.narrow[2 *  1 + 0])
#define a10h   (kc->u.narrow[2 *  1 + 1])
#define a20l   (kc->u.narrow[2 *  2 + 0])
#define a20h   (kc->u.narrow[2 *  2 + 1])
#define a30l   (kc->u.narrow[2 *  3 + 0])
#define a30h   (kc->u.narrow[2 *  3 + 1])
#define a40l   (kc->u.narrow[2 *  4 + 0])
#define a40h   (kc->u.narrow[2 *  4 + 1])
#define a01l   (kc->u.narrow[2 *  5 + 0])
#define a01h   (kc->u.narrow[2 *  5 + 1])
#define a11l   (kc->u.narrow[2 *  6 + 0])
#define a11h   (kc->u.narrow[2 *  6 + 1])
#define a21l   (kc->u.narrow[2 *  7 + 0])
#define a21h   (kc->u.narrow[2 *  7 + 1])
#define a31l   (kc->u.narrow[2 *  8 + 0])
#define a31h   (kc->u.narrow[2 *  8 + 1])
#define a41l   (kc->u.narrow[2 *  9 + 0])
#define a41h   (kc->u.narrow[2 *  9 + 1])
#define a02l   (kc->u.narrow[2 * 10 + 0])
#define a02h   (kc->u.narrow[2 * 10 + 1])
#define a12l   (kc->u.narrow[2 * 11 + 0])
#define a12h   (kc->u.narrow[2 * 11 + 1])
#define a22l   (kc->u.narrow[2 * 12 + 0])
#define a22h   (kc->u.narrow[2 * 12 + 1])
#define a32l   (kc->u.narrow[2 * 13 + 0])
#define a32h   (kc->u.narrow[2 * 13 + 1])
#define a42l   (kc->u.narrow[2 * 14 + 0])
#define a42h   (kc->u.narrow[2 * 14 + 1])
#define a03l   (kc->u.narrow[2 * 15 + 0])
#define a03h   (kc->u.narrow[2 * 15 + 1])
#define a13l   (kc->u.narrow[2 * 16 + 0])
#define a13h   (kc->u.narrow[2 * 16 + 1])
#define a23l   (kc->u.narrow[2 * 17 + 0])
#define a23h   (kc->u.narrow[2 * 17 + 1])
#define a33l   (kc->u.narrow[2 * 18 + 0])
#define a33h   (kc->u.narrow[2 * 18 + 1])
#define a43l   (kc->u.narrow[2 * 19 + 0])
#define a43h   (kc->u.narrow[2 * 19 + 1])
#define a04l   (kc->u.narrow[2 * 20 + 0])
#define a04h   (kc->u.narrow[2 * 20 + 1])
#define a14l   (kc->u.narrow[2 * 21 + 0])
#define a14h   (kc->u.narrow[2 * 21 + 1])
#define a24l   (kc->u.narrow[2 * 22 + 0])
#define a24h   (kc->u.narrow[2 * 22 + 1])
#define a34l   (kc->u.narrow[2 * 23 + 0])
#define a34h   (kc->u.narrow[2 * 23 + 1])
#define a44l   (kc->u.narrow[2 * 24 + 0])
#define a44h   (kc->u.narrow[2 * 24 + 1])

#define DECL_STATE
#define READ_STATE(state)
#define WRITE_STATE(state)

#define INPUT_BUF(size)   do { \
		size_t j; \
		for (j = 0; j < (size); j += 8) { \
			sph_u32 tl, th; \
			tl = sph_dec32le_aligned(buf + j + 0); \
			th = sph_dec32le_aligned(buf + j + 4); \
			INTERLEAVE(tl, th); \
			kc->u.narrow[(j >> 2) + 0] ^= tl; \
			kc->u.narrow[(j >> 2) + 1] ^= th; \
		} \
	} while (0)

#define INPUT_BUF144   INPUT_BUF(144)
#define INPUT_BUF136   INPUT_BUF(136)
#define INPUT_BUF104   INPUT_BUF(104)
#define INPUT_BUF72    INPUT_BUF(72)

#else

#define DECL_STATE \
	sph_u32 a00l, a00h, a01l, a01h, a02l, a02h, a03l, a03h, a04l, a04h; \
	sph_u32 a10l, a10h, a11l, a11h, a12l, a12h, a13l, a13h, a14l, a14h; \
	sph_u32 a20l, a20h, a21l, a21h, a22l, a22h, a23l, a23h, a24l, a24h; \
	sph_u32 a30l, a30h, a31l, a31h, a32l, a32h, a33l, a33h, a34l, a34h; \
	sph_u32 a40l, a40h, a41l, a41h, a42l, a42h, a43l, a43h, a44l, a44h;

#define READ_STATE(state)   do { \
		a00l = (state)->u.narrow[2 *  0 + 0]; \
		a00h = (state)->u.narrow[2 *  0 + 1]; \
		a10l = (state)->u.narrow[2 *  1 + 0]; \
		a10h = (state)->u.narrow[2 *  1 + 1]; \
		a20l = (state)->u.narrow[2 *  2 + 0]; \
		a20h = (state)->u.narrow[2 *  2 + 1]; \
		a30l = (state)->u.narrow[2 *  3 + 0]; \
		a30h = (state)->u.narrow[2 *  3 + 1]; \
		a40l = (state)->u.narrow[2 *  4 + 0]; \
		a40h = (state)->u.narrow[2 *  4 + 1]; \
		a01l = (state)->u.narrow[2 *  5 + 0]; \
		a01h = (state)->u.narrow[2 *  5 + 1]; \
		a11l = (state)->u.narrow[2 *  6 + 0]; \
		a11h = (state)->u.narrow[2 *  6 + 1]; \
		a21l = (state)->u.narrow[2 *  7 + 0]; \
		a21h = (state)->u.narrow[2 *  7 + 1]; \
		a31l = (state)->u.narrow[2 *  8 + 0]; \
		a31h = (state)->u.narrow[2 *  8 + 1]; \
		a41l = (state)->u.narrow[2 *  9 + 0]; \
		a41h = (state)->u.narrow[2 *  9 + 1]; \
		a02l = (state)->u.narrow[2 * 10 + 0]; \
		a02h = (state)->u.narrow[2 * 10 + 1]; \
		a12l = (state)->u.narrow[2 * 11 + 0]; \
		a12h = (state)->u.narrow[2 * 11 + 1]; \
		a22l = (state)->u.narrow[2 * 12 + 0]; \
		a22h = (state)->u.narrow[2 * 12 + 1]; \
		a32l = (state)->u.narrow[2 * 13 + 0]; \
		a32h = (state)->u.narrow[2 * 13 + 1]; \
		a42l = (state)->u.narrow[2 * 14 + 0]; \
		a42h = (state)->u.narrow[2 * 14 + 1]; \
		a03l = (state)->u.narrow[2 * 15 + 0]; \
		a03h = (state)->u.narrow[2 * 15 + 1]; \
		a13l = (state)->u.narrow[2 * 16 + 0]; \
		a13h = (state)->u.narrow[2 * 16 + 1]; \
		a23l = (state)->u.narrow[2 * 17 + 0]; \
		a23h = (state)->u.narrow[2 * 17 + 1]; \
		a33l = (state)->u.narrow[2 * 18 + 0]; \
		a33h = (state)->u.narrow[2 * 18 + 1]; \
		a43l = (state)->u.narrow[2 * 19 + 0]; \
		a43h = (state)->u.narrow[2 * 19 + 1]; \
		a04l = (state)->u.narrow[2 * 20 + 0]; \
		a04h = (state)->u.narrow[2 * 20 + 1]; \
		a14l = (state)->u.narrow[2 * 21 + 0]; \
		a14h = (state)->u.narrow[2 * 21 + 1]; \
		a24l = (state)->u.narrow[2 * 22 + 0]; \
		a24h = (state)->u.narrow[2 * 22 + 1]; \
		a34l = (state)->u.narrow[2 * 23 + 0]; \
		a34h = (state)->u.narrow[2 * 23 + 1]; \
		a44l = (state)->u.narrow[2 * 24 + 0]; \
		a44h = (state)->u.narrow[2 * 24 + 1]; \
	} while (0)

#define WRITE_STATE(state)   do { \
		(state)->u.narrow[2 *  0 + 0] = a00l; \
		(state)->u.narrow[2 *  0 + 1] = a00h; \
		(state)->u.narrow[2 *  1 + 0] = a10l; \
		(state)->u.narrow[2 *  1 + 1] = a10h; \
		(state)->u.narrow[2 *  2 + 0] = a20l; \
		(state)->u.narrow[2 *  2 + 1] = a20h; \
		(state)->u.narrow[2 *  3 + 0] = a30l; \
		(state)->u.narrow[2 *  3 + 1] = a30h; \
		(state)->u.narrow[2 *  4 + 0] = a40l; \
		(state)->u.narrow[2 *  4 + 1] = a40h; \
		(state)->u.narrow[2 *  5 + 0] = a01l; \
		(state)->u.narrow[2 *  5 + 1] = a01h; \
		(state)->u.narrow[2 *  6 + 0] = a11l; \
		(state)->u.narrow[2 *  6 + 1] = a11h; \
		(state)->u.narrow[2 *  7 + 0] = a21l; \
		(state)->u.narrow[2 *  7 + 1] = a21h; \
		(state)->u.narrow[2 *  8 + 0] = a31l; \
		(state)->u.narrow[2 *  8 + 1] = a31h; \
		(state)->u.narrow[2 *  9 + 0] = a41l; \
		(state)->u.narrow[2 *  9 + 1] = a41h; \
		(state)->u.narrow[2 * 10 + 0] = a02l; \
		(state)->u.narrow[2 * 10 + 1] = a02h; \
		(state)->u.narrow[2 * 11 + 0] = a12l; \
		(state)->u.narrow[2 * 11 + 1] = a12h; \
		(state)->u.narrow[2 * 12 + 0] = a22l; \
		(state)->u.narrow[2 * 12 + 1] = a22h; \
		(state)->u.narrow[2 * 13 + 0] = a32l; \
		(state)->u.narrow[2 * 13 + 1] = a32h; \
		(state)->u.narrow[2 * 14 + 0] = a42l; \
		(state)->u.narrow[2 * 14 + 1] = a42h; \
		(state)->u.narrow[2 * 15 + 0] = a03l; \
		(state)->u.narrow[2 * 15 + 1] = a03h; \
		(state)->u.narrow[2 * 16 + 0] = a13l; \
		(state)->u.narrow[2 * 16 + 1] = a13h; \
		(state)->u.narrow[2 * 17 + 0] = a23l; \
		(state)->u.narrow[2 * 17 + 1] = a23h; \
		(state)->u.narrow[2 * 18 + 0] = a33l; \
		(state)->u.narrow[2 * 18 + 1] = a33h; \
		(state)->u.narrow[2 * 19 + 0] = a43l; \
		(state)->u.narrow[2 * 19 + 1] = a43h; \
		(state)->u.narrow[2 * 20 + 0] = a04l; \
		(state)->u.narrow[2 * 20 + 1] = a04h; \
		(state)->u.narrow[2 * 21 + 0] = a14l; \
		(state)->u.narrow[2 * 21 + 1] = a14h; \
		(state)->u.narrow[2 * 22 + 0] = a24l; \
		(state)->u.narrow[2 * 22 + 1] = a24h; \
		(state)->u.narrow[2 * 23 + 0] = a34l; \
		(state)->u.narrow[2 * 23 + 1] = a34h; \
		(state)->u.narrow[2 * 24 + 0] = a44l; \
		(state)->u.narrow[2 * 24 + 1] = a44h; \
	} while (0)

#define READ64(d, off)   do { \
		sph_u32 tl, th; \
		tl = sph_dec32le_aligned(buf + (off)); \
		th = sph_dec32le_aligned(buf + (off) + 4); \
		INTERLEAVE(tl, th); \
		d ## l ^= tl; \
		d ## h ^= th; \
	} while (0)

#define INPUT_BUF144   do { \
		READ64(a00,   0); \
		READ64(a10,   8); \
		READ64(a20,  16); \
		READ64(a30,  24); \
		READ64(a40,  32); \
		READ64(a01,  40); \
		READ64(a11,  48); \
		READ64(a21,  56); \
		READ64(a31,  64); \
		READ64(a41,  72); \
		READ64(a02,  80); \
		READ64(a12,  88); \
		READ64(a22,  96); \
		READ64(a32, 104); \
		READ64(a42, 112); \
		READ64(a03, 120); \
		READ64(a13, 128); \
		READ64(a23, 136); \
	} while (0)

#define INPUT_BUF136   do { \
		READ64(a00,   0); \
		READ64(a10,   8); \
		READ64(a20,  16); \
		READ64(a30,  24); \
		READ64(a40,  32); \
		READ64(a01,  40); \
		READ64(a11,  48); \
		READ64(a21,  56); \
		READ64(a31,  64); \
		READ64(a41,  72); \
		READ64(a02,  80); \
		READ64(a12,  88); \
		READ64(a22,  96); \
		READ64(a32, 104); \
		READ64(a42, 112); \
		READ64(a03, 120); \
		READ64(a13, 128); \
	} while (0)

#define INPUT_BUF104   do { \
		READ64(a00,   0); \
		READ64(a10,   8); \
		READ64(a20,  16); \
		READ64(a30,  24); \
		READ64(a40,  32); \
		READ64(a01,  40); \
		READ64(a11,  48); \
		READ64(a21,  56); \
		READ64(a31,  64); \
		READ64(a41,  72); \
		READ64(a02,  80); \
		READ64(a12,  88); \
		READ64(a22,  96); \
	} while (0)

#define INPUT_BUF72   do { \
		READ64(a00,   0); \
		READ64(a10,   8); \
		READ64(a20,  16); \
		READ64(a30,  24); \
		READ64(a40,  32); \
		READ64(a01,  40); \
		READ64(a11,  48); \
		READ64(a21,  56); \
		READ64(a31,  64); \
	} while (0)

#define INPUT_BUF(lim)   do { \
		READ64(a00,   0); \
		READ64(a10,   8); \
		READ64(a20,  16); \
		READ64(a30,  24); \
		READ64(a40,  32); \
		READ64(a01,  40); \
		READ64(a11,  48); \
		READ64(a21,  56); \
		READ64(a31,  64); \
		if ((lim) == 72) \
			break; \
		READ64(a41,  72); \
		READ64(a02,  80); \
		READ64(a12,  88); \
		READ64(a22,  96); \
		if ((lim) == 104) \
			break; \
		READ64(a32, 104); \
		READ64(a42, 112); \
		READ64(a03, 120); \
		READ64(a13, 128); \
		if ((lim) == 136) \
			break; \
		READ64(a23, 136); \
	} while (0)

#endif

#define DECL64(x)        sph_u64 x ## l, x ## h
#define MOV64(d, s)      (d ## l = s ## l, d ## h = s ## h)
#define XOR64(d, a, b)   (d ## l = a ## l ^ b ## l, d ## h = a ## h ^ b ## h)
#define AND64(d, a, b)   (d ## l = a ## l & b ## l, d ## h = a ## h & b ## h)
#define OR64(d, a, b)    (d ## l = a ## l | b ## l, d ## h = a ## h | b ## h)
#define NOT64(d, s)      (d ## l = SPH_T32(~s ## l), d ## h = SPH_T32(~s ## h))
#define ROL64(d, v, n)   ROL64_ ## n(d, v)

#if SPH_KECCAK_INTERLEAVE

#define ROL64_odd1(d, v)   do { \
		sph_u32 tmp; \
		tmp = v ## l; \
		d ## l = SPH_T32(v ## h << 1) | (v ## h >> 31); \
		d ## h = tmp; \
	} while (0)

#define ROL64_odd63(d, v)   do { \
		sph_u32 tmp; \
		tmp = SPH_T32(v ## l << 31) | (v ## l >> 1); \
		d ## l = v ## h; \
		d ## h = tmp; \
	} while (0)

#define ROL64_odd(d, v, n)   do { \
		sph_u32 tmp; \
		tmp = SPH_T32(v ## l << (n - 1)) | (v ## l >> (33 - n)); \
		d ## l = SPH_T32(v ## h << n) | (v ## h >> (32 - n)); \
		d ## h = tmp; \
	} while (0)

#define ROL64_even(d, v, n)   do { \
		d ## l = SPH_T32(v ## l << n) | (v ## l >> (32 - n)); \
		d ## h = SPH_T32(v ## h << n) | (v ## h >> (32 - n)); \
	} while (0)

#define ROL64_0(d, v)
#define ROL64_1(d, v)    ROL64_odd1(d, v)
#define ROL64_2(d, v)    ROL64_even(d, v,  1)
#define ROL64_3(d, v)    ROL64_odd( d, v,  2)
#define ROL64_4(d, v)    ROL64_even(d, v,  2)
#define ROL64_5(d, v)    ROL64_odd( d, v,  3)
#define ROL64_6(d, v)    ROL64_even(d, v,  3)
#define ROL64_7(d, v)    ROL64_odd( d, v,  4)
#define ROL64_8(d, v)    ROL64_even(d, v,  4)
#define ROL64_9(d, v)    ROL64_odd( d, v,  5)
#define ROL64_10(d, v)   ROL64_even(d, v,  5)
#define ROL64_11(d, v)   ROL64_odd( d, v,  6)
#define ROL64_12(d, v)   ROL64_even(d, v,  6)
#define ROL64_13(d, v)   ROL64_odd( d, v,  7)
#define ROL64_14(d, v)   ROL64_even(d, v,  7)
#define ROL64_15(d, v)   ROL64_odd( d, v,  8)
#define ROL64_16(d, v)   ROL64_even(d, v,  8)
#define ROL64_17(d, v)   ROL64_odd( d, v,  9)
#define ROL64_18(d, v)   ROL64_even(d, v,  9)
#define ROL64_19(d, v)   ROL64_odd( d, v, 10)
#define ROL64_20(d, v)   ROL64_even(d, v, 10)
#define ROL64_21(d, v)   ROL64_odd( d, v, 11)
#define ROL64_22(d, v)   ROL64_even(d, v, 11)
#define ROL64_23(d, v)   ROL64_odd( d, v, 12)
#define ROL64_24(d, v)   ROL64_even(d, v, 12)
#define ROL64_25(d, v)   ROL64_odd( d, v, 13)
#define ROL64_26(d, v)   ROL64_even(d, v, 13)
#define ROL64_27(d, v)   ROL64_odd( d, v, 14)
#define ROL64_28(d, v)   ROL64_even(d, v, 14)
#define ROL64_29(d, v)   ROL64_odd( d, v, 15)
#define ROL64_30(d, v)   ROL64_even(d, v, 15)
#define ROL64_31(d, v)   ROL64_odd( d, v, 16)
#define ROL64_32(d, v)   ROL64_even(d, v, 16)
#define ROL64_33(d, v)   ROL64_odd( d, v, 17)
#define ROL64_34(d, v)   ROL64_even(d, v, 17)
#define ROL64_35(d, v)   ROL64_odd( d, v, 18)
#define ROL64_36(d, v)   ROL64_even(d, v, 18)
#define ROL64_37(d, v)   ROL64_odd( d, v, 19)
#define ROL64_38(d, v)   ROL64_even(d, v, 19)
#define ROL64_39(d, v)   ROL64_odd( d, v, 20)
#define ROL64_40(d, v)   ROL64_even(d, v, 20)
#define ROL64_41(d, v)   ROL64_odd( d, v, 21)
#define ROL64_42(d, v)   ROL64_even(d, v, 21)
#define ROL64_43(d, v)   ROL64_odd( d, v, 22)
#define ROL64_44(d, v)   ROL64_even(d, v, 22)
#define ROL64_45(d, v)   ROL64_odd( d, v, 23)
#define ROL64_46(d, v)   ROL64_even(d, v, 23)
#define ROL64_47(d, v)   ROL64_odd( d, v, 24)
#define ROL64_48(d, v)   ROL64_even(d, v, 24)
#define ROL64_49(d, v)   ROL64_odd( d, v, 25)
#define ROL64_50(d, v)   ROL64_even(d, v, 25)
#define ROL64_51(d, v)   ROL64_odd( d, v, 26)
#define ROL64_52(d, v)   ROL64_even(d, v, 26)
#define ROL64_53(d, v)   ROL64_odd( d, v, 27)
#define ROL64_54(d, v)   ROL64_even(d, v, 27)
#define ROL64_55(d, v)   ROL64_odd( d, v, 28)
#define ROL64_56(d, v)   ROL64_even(d, v, 28)
#define ROL64_57(d, v)   ROL64_odd( d, v, 29)
#define ROL64_58(d, v)   ROL64_even(d, v, 29)
#define ROL64_59(d, v)   ROL64_odd( d, v, 30)
#define ROL64_60(d, v)   ROL64_even(d, v, 30)
#define ROL64_61(d, v)   ROL64_odd( d, v, 31)
#define ROL64_62(d, v)   ROL64_even(d, v, 31)
#define ROL64_63(d, v)   ROL64_odd63(d, v)

#else

#define ROL64_small(d, v, n)   do { \
		sph_u32 tmp; \
		tmp = SPH_T32(v ## l << n) | (v ## h >> (32 - n)); \
		d ## h = SPH_T32(v ## h << n) | (v ## l >> (32 - n)); \
		d ## l = tmp; \
	} while (0)

#define ROL64_0(d, v)    0
#define ROL64_1(d, v)    ROL64_small(d, v, 1)
#define ROL64_2(d, v)    ROL64_small(d, v, 2)
#define ROL64_3(d, v)    ROL64_small(d, v, 3)
#define ROL64_4(d, v)    ROL64_small(d, v, 4)
#define ROL64_5(d, v)    ROL64_small(d, v, 5)
#define ROL64_6(d, v)    ROL64_small(d, v, 6)
#define ROL64_7(d, v)    ROL64_small(d, v, 7)
#define ROL64_8(d, v)    ROL64_small(d, v, 8)
#define ROL64_9(d, v)    ROL64_small(d, v, 9)
#define ROL64_10(d, v)   ROL64_small(d, v, 10)
#define ROL64_11(d, v)   ROL64_small(d, v, 11)
#define ROL64_12(d, v)   ROL64_small(d, v, 12)
#define ROL64_13(d, v)   ROL64_small(d, v, 13)
#define ROL64_14(d, v)   ROL64_small(d, v, 14)
#define ROL64_15(d, v)   ROL64_small(d, v, 15)
#define ROL64_16(d, v)   ROL64_small(d, v, 16)
#define ROL64_17(d, v)   ROL64_small(d, v, 17)
#define ROL64_18(d, v)   ROL64_small(d, v, 18)
#define ROL64_19(d, v)   ROL64_small(d, v, 19)
#define ROL64_20(d, v)   ROL64_small(d, v, 20)
#define ROL64_21(d, v)   ROL64_small(d, v, 21)
#define ROL64_22(d, v)   ROL64_small(d, v, 22)
#define ROL64_23(d, v)   ROL64_small(d, v, 23)
#define ROL64_24(d, v)   ROL64_small(d, v, 24)
#define ROL64_25(d, v)   ROL64_small(d, v, 25)
#define ROL64_26(d, v)   ROL64_small(d, v, 26)
#define ROL64_27(d, v)   ROL64_small(d, v, 27)
#define ROL64_28(d, v)   ROL64_small(d, v, 28)
#define ROL64_29(d, v)   ROL64_small(d, v, 29)
#define ROL64_30(d, v)   ROL64_small(d, v, 30)
#define ROL64_31(d, v)   ROL64_small(d, v, 31)

#define ROL64_32(d, v)   do { \
		sph_u32 tmp; \
		tmp = v ## l; \
		d ## l = v ## h; \
		d ## h = tmp; \
	} while (0)

#define ROL64_big(d, v, n)   do { \
		sph_u32 trl, trh; \
		ROL64_small(tr, v, n); \
		d ## h = trl; \
		d ## l = trh; \
	} while (0)

#define ROL64_33(d, v)   ROL64_big(d, v, 1)
#define ROL64_34(d, v)   ROL64_big(d, v, 2)
#define ROL64_35(d, v)   ROL64_big(d, v, 3)
#define ROL64_36(d, v)   ROL64_big(d, v, 4)
#define ROL64_37(d, v)   ROL64_big(d, v, 5)
#define ROL64_38(d, v)   ROL64_big(d, v, 6)
#define ROL64_39(d, v)   ROL64_big(d, v, 7)
#define ROL64_40(d, v)   ROL64_big(d, v, 8)
#define ROL64_41(d, v)   ROL64_big(d, v, 9)
#define ROL64_42(d, v)   ROL64_big(d, v, 10)
#define ROL64_43(d, v)   ROL64_big(d, v, 11)
#define ROL64_44(d, v)   ROL64_big(d, v, 12)
#define ROL64_45(d, v)   ROL64_big(d, v, 13)
#define ROL64_46(d, v)   ROL64_big(d, v, 14)
#define ROL64_47(d, v)   ROL64_big(d, v, 15)
#define ROL64_48(d, v)   ROL64_big(d, v, 16)
#define ROL64_49(d, v)   ROL64_big(d, v, 17)
#define ROL64_50(d, v)   ROL64_big(d, v, 18)
#define ROL64_51(d, v)   ROL64_big(d, v, 19)
#define ROL64_52(d, v)   ROL64_big(d, v, 20)
#define ROL64_53(d, v)   ROL64_big(d, v, 21)
#define ROL64_54(d, v)   ROL64_big(d, v, 22)
#define ROL64_55(d, v)   ROL64_big(d, v, 23)
#define ROL64_56(d, v)   ROL64_big(d, v, 24)
#define ROL64_57(d, v)   ROL64_big(d, v, 25)
#define ROL64_58(d, v)   ROL64_big(d, v, 26)
#define ROL64_59(d, v)   ROL64_big(d, v, 27)
#define ROL64_60(d, v)   ROL64_big(d, v, 28)
#