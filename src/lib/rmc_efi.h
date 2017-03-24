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

/*
 * EFI Definitions
 * It doesn't mean we have an EFI implementation in RMC.
 * We only provide what we need for EFI work (e.g. parsing sys
 * configuration table to get SMBIOS data.
 *
 * We don't want to bring such dependency for rmc's sake to a
 * client which is based on a different EFI implementation or
 * compile setup.
 *
 * This header file shall be internally used in rmc.
 */

#ifndef INC_RMC_EFI_H_
#define INC_RMC_EFI_H_

#ifdef RMC_EFI
#include <rmc_types.h>

typedef unsigned long int rmc_uintn_t;
typedef rmc_uint64_t rmc_uint64_t;
typedef rmc_uint32_t rmc_uint32_t;
typedef rmc_uint16_t rmc_uint16_t;
typedef rmc_uint16_t rmc_uint16_t;
typedef rmc_uint8_t rmc_uint8_t;

typedef void * EFI_HANDLE;
typedef struct {
    rmc_uint32_t d1;
    rmc_uint16_t d2;
    rmc_uint16_t d3;
    rmc_uint8_t d4[8];
} EFI_GUID;

/* Pointers as place holders */
typedef void * EFI_SIMPLE_TEXT_INPUT_PROTOCOL_P;
typedef void * EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_P;
typedef void * EFI_RUNTIME_SERVICES_P;
typedef void * EFI_BOOT_SERVICES_P;

typedef struct {
    rmc_uint64_t Signature;
    rmc_uint32_t Revision;
    rmc_uint32_t HeaderSize;
    rmc_uint32_t CRC32;
    rmc_uint32_t Reserved;
} EFI_TABLE_HEADER;

typedef struct {
    EFI_GUID VendorGuid;
    void *VendorTable;
} EFI_CONFIGURATION_TABLE;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    rmc_uint16_t *FirmwareVendor;
    rmc_uint32_t FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL_P ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_P ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_P StdErr;
    EFI_RUNTIME_SERVICES_P RuntimeServices;
    EFI_BOOT_SERVICES_P *BootServices;
    rmc_uintn_t NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE *ConfigurationTable;
} EFI_SYSTEM_TABLE;
#endif
#endif /* INC_RMC_EFI_H_ */
