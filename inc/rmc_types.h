/*
 * Copyright (c) 2016 - 2017 Intel Corporation.
 *
 * Author: Jianxun Zhang <jianxun.zhang@intel.com>
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
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef INC_RMC_TYPES_H_
#define INC_RMC_TYPES_H_

#ifndef RMC_EFI
#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef uint8_t rmc_uint8_t;
typedef uint16_t rmc_uint16_t;
typedef uint32_t rmc_uint32_t;
typedef uint64_t rmc_uint64_t;
typedef size_t rmc_size_t;
typedef ssize_t rmc_ssize_t;

#else
/* we specify -nostdinc in C flag and provide these in rmc
 * for EFI applications that don't want to use standard headers.
 */
#ifdef __GNUC__
#ifndef NULL
#define NULL 0
#endif
typedef unsigned int            rmc_uint32_t;
typedef unsigned short int      rmc_uint16_t;
typedef unsigned char           rmc_uint8_t;
#ifdef __x86_64__
typedef unsigned long int       rmc_uint64_t;
#elif defined(__i386__)
__extension__
typedef unsigned long long int  rmc_uint64_t;
#else
#error "rmc only supports 32 and 64 bit x86 platforms"
#endif
#else
#error "rmc needs gcc compiler"
#endif /* __GNUC__ */
typedef unsigned long rmc_size_t;
typedef long rmc_ssize_t;
#endif /* RMC_EFI */

#endif /* INC_RMC_TYPES_H_ */
