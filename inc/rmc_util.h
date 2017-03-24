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

/* util.c - implementation of basic C functions
 * In EFI land, we don't have all of these functions as what we do
 * from std C lib.
 */

#ifndef INC_RMC_UTIL_H_
#define INC_RMC_UTIL_H_
#include <rmc_types.h>

void *memset(void *s, rmc_uint8_t c, rmc_size_t n);

char *strncpy(char *dest, const char *src, rmc_size_t n);

char *strcpy(char *dest, const char *src);

rmc_size_t strlen(const char *s);

void *memcpy(void *dest, const void *src, rmc_size_t n);

int strncmp(const char *s1, const char *s2, rmc_size_t n);

#endif
