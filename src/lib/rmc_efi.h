/* Copyright (C) 2016 Jianxun Zhang <jianxun.zhang@intel.com>
 *
 * EFI Definitions
 * It doesn't mean we have an EFI implementation in RMC.
 * We only provide what we need for EFI work (e.g. parsing sys
 * configuration table to get SMBIOS data.
 *
 * We don't want to bring such dependency for rmc's sake to a
 * client which is based on a different EFI implementation.
 *
 * We have to be more self-contained at this point...
 */

#ifndef INC_RMC_EFI_H_
#define INC_RMC_EFI_H_

#ifdef RMC_EFI
#include <rmc_types.h>

typedef unsigned long int UINTN;
typedef uint64_t UINT64;
typedef uint32_t UINT32;
typedef uint16_t UINT16;
typedef uint16_t CHAR16;
typedef uint8_t UINT8;

typedef void * EFI_HANDLE;
typedef struct {
    UINT32 d1;
    UINT16 d2;
    UINT16 d3;
    UINT8 d4[8];
} EFI_GUID;

/* Fake place holder for pointers */
typedef void * EFI_SIMPLE_TEXT_INPUT_PROTOCOL_P;
typedef void * EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_P;
typedef void * EFI_RUNTIME_SERVICES_P;
typedef void * EFI_BOOT_SERVICES_P;

typedef struct {
    UINT64 Signature;
    UINT32 Revision;
    UINT32 HeaderSize;
    UINT32 CRC32;
    UINT32 Reserved;
} EFI_TABLE_HEADER;

typedef struct {
    EFI_GUID VendorGuid;
    void *VendorTable;
} EFI_CONFIGURATION_TABLE;

typedef struct {
    EFI_TABLE_HEADER Hdr;
    CHAR16 *FirmwareVendor;
    UINT32 FirmwareRevision;
    EFI_HANDLE ConsoleInHandle;
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL_P ConIn;
    EFI_HANDLE ConsoleOutHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_P ConOut;
    EFI_HANDLE StandardErrorHandle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL_P StdErr;
    EFI_RUNTIME_SERVICES_P RuntimeServices;
    EFI_BOOT_SERVICES_P *BootServices;
    UINTN NumberOfTableEntries;
    EFI_CONFIGURATION_TABLE *ConfigurationTable;
} EFI_SYSTEM_TABLE;
#endif
#endif /* INC_RMC_EFI_H_ */
