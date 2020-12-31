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

static const sph_u64 RC[