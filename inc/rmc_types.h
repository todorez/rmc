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
#include <efi.h>
/* we specify -nostdinc in C flag and provide these in rmc
 * for EFI applications that don't want to use standard headers.
 */
typedef unsigned long size_t;
typedef long ssize_t;
#endif

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;


#endif /* INC_RMC_TYPES_H_ */
