/* Copyright (C) 2016 Jianxun Zhang <jianxun.zhang@intel.com>
 *
 * rmc_types.h
 */

#ifndef INC_RMC_TYPES_H_
#define INC_RMC_TYPES_H_

#ifndef RMC_EFI
#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#else
/* we specify -nostdinc in C flag and provide these in rmc
 * for EFI applications that don't want to use standard headers.
 */
#ifdef __GNUC__
#ifndef NULL
#define NULL 0
#endif
typedef unsigned int            uint32_t;
typedef unsigned short int      uint16_t;
typedef unsigned char           uint8_t;
#ifdef __x86_64__
typedef unsigned long int       uint64_t;
#elif defined(__i386__)
__extension__
typedef unsigned long long int  uint64_t;
#else
#error "rmc only supports 32 and 64 bit x86 platforms"
#endif
#else
#error "rmc needs gcc compiler"
#endif

typedef unsigned long size_t;
typedef long ssize_t;
#endif

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;


#endif /* INC_RMC_TYPES_H_ */
