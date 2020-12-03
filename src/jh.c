
/* $Id: jh.c 255 2011-06-07 19:50:20Z tp $ */
/*
 * JH implementation.
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

#include "sph_jh.h"

#ifdef __cplusplus
extern "C"{
#endif


#if SPH_SMALL_FOOTPRINT && !defined SPH_SMALL_FOOTPRINT_JH
#define SPH_SMALL_FOOTPRINT_JH   1
#endif

#if !defined SPH_JH_64 && SPH_64_TRUE
#define SPH_JH_64   1
#endif

#if !SPH_64
#undef SPH_JH_64
#endif

#ifdef _MSC_VER
#pragma warning (disable: 4146)
#endif

/*
 * The internal bitslice representation may use either big-endian or
 * little-endian (true bitslice operations do not care about the bit
 * ordering, and the bit-swapping linear operations in JH happen to
 * be invariant through endianness-swapping). The constants must be
 * defined according to the chosen endianness; we use some
 * byte-swapping macros for that.
 */

#if SPH_LITTLE_ENDIAN

#define C32e(x)     ((SPH_C32(x) >> 24) \
                    | ((SPH_C32(x) >>  8) & SPH_C32(0x0000FF00)) \
                    | ((SPH_C32(x) <<  8) & SPH_C32(0x00FF0000)) \
                    | ((SPH_C32(x) << 24) & SPH_C32(0xFF000000)))
#define dec32e_aligned   sph_dec32le_aligned
#define enc32e           sph_enc32le

#if SPH_64