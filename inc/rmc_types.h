/* Copyright (C) 2016 Jianxun Zhang <jianxun.zhang@intel.com>
 *
 * rmc_types.h
 */

#ifndef INC_RMC_TYPES_H_
#define INC_RMC_TYPES_H_

#ifndef RMC_EFI
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#else
#include <efi.h>
/* Fixme: we define (s)size_t here for both 32 and 64 bit because gnu-efi doesn't provide these */
typedef uint64_t size_t;
typedef uint64_t ssize_t;
#endif

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t QWORD;


#endif /* INC_RMC_TYPES_H_ */
