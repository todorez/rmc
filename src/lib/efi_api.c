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

/* RMC API implementation for EFI context */

#include <rmcl.h>
#include <rsmp.h>
#include "rmc_efi.h"

static EFI_GUID smbios3_guid = { 0xf2fd1544, 0x9794, 0x4a2c, \
                               {0x99, 0x2e, 0xe5, 0xbb, 0xcf, 0x20, 0xe3, 0x94}};
static EFI_GUID smbios_guid = {0xeb9d2d31,0x2d88,0x11d3, \
                              {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4d}};
/* Compare if two GUIDs
 * return: 0 when they are identical
 */
static int compare_guid (EFI_GUID *guid1, EFI_GUID *guid2) {
    return (guid1->d1 - guid2->d1 \
            || guid1->d2 - guid2->d2 \
            || guid1->d3 - guid2->d3 \
            || guid1->d4[0] - guid2->d4[0] \
            || guid1->d4[1] - guid2->d4[1] \
            || guid1->d4[2] - guid2->d4[2] \
            || guid1->d4[3] - guid2->d4[3] \
            || guid1->d4[4] - guid2->d4[4] \
            || guid1->d4[5] - guid2->d4[5] \
            || guid1->d4[6] - guid2->d4[6] \
            || guid1->d4[7] - guid2->d4[7] \
    );
}

static void *get_smbios_entry(void * sys_config_table) {
    EFI_SYSTEM_TABLE *systab = (EFI_SYSTEM_TABLE *)sys_config_table;
    rmc_uintn_t i;
    EFI_CONFIGURATION_TABLE *entry;

    if (!systab)
        return NULL;

    entry = systab->ConfigurationTable;

    for (i = 0; i < systab->NumberOfTableEntries && entry; i++, entry++) {
        if (!compare_guid(&smbios3_guid, &entry->VendorGuid) \
            || !compare_guid(&smbios_guid, &entry->VendorGuid))
            return entry->VendorTable;
    }

    return NULL;
}

int rmc_get_fingerprint(void *sys_table, rmc_fingerprint_t *fp) {
    void *smbios_entry = NULL;
    rmc_uint64_t smbios_struct_addr = 0;
    rmc_uint16_t smbios_struct_len = 0;
    rmc_uint8_t *smbios_struct_start = NULL;

    if (!fp)
        return 1;

    smbios_entry = get_smbios_entry(sys_table);

    if (!smbios_entry)
        return 1;

    if (rsmp_get_smbios_strcut(smbios_entry, &smbios_struct_addr, &smbios_struct_len))
        return 1;

    /* To avoid compiler warning for 32 bit build */
    smbios_struct_start += smbios_struct_addr;

    return rsmp_get_fingerprint_from_smbios_struct(smbios_struct_start, fp);
}

int rmc_query_file_by_fp(rmc_fingerprint_t *fp, rmc_uint8_t *db_blob, char *file_name, rmc_file_t *file) {
    return query_policy_from_db(fp, db_blob, RMC_GENERIC_FILE, file_name, file);
}

int rmc_gimme_file(void *sys_table, rmc_uint8_t *db_blob, char *file_name, rmc_file_t *file) {
    rmc_fingerprint_t fp;

    if (!sys_table || !db_blob || !file_name || !file)
        return 1;

    if (rmc_get_fingerprint(sys_table, &fp))
        return 1;

    if (rmc_query_file_by_fp(&fp, db_blob, file_name, file))
        return 1;
    else
        return 0;
}
