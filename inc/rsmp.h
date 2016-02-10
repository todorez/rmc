/* Copyright (C) 2016 Jianxun Zhang <jianxun.zhang@intel.com>
 *
 * header file for standard smbios structure definitions
 * Reference: SMBIOS Reference Specification 3.0.0
 */
#ifndef INC_RSMP_BIOS_H_
#define INC_RSMP_H_

#include <stdint.h>
#include <rmc_types.h>
#include <rmcl.h>

#pragma pack(1)

/*
 * SMBIOS Entry Point Structure
 */

typedef union smbios_ep {
    /* */
    struct {
        BYTE ep_anchor[4];
        BYTE ep_chksum;
        BYTE ep_len;
        BYTE major_ver;
        BYTE minor_ver;
        WORD max_struct_size;
        BYTE ep_rev;
        BYTE fmt_area[5];
        BYTE interm_anchor[5];
        BYTE interm_chksum;
        WORD struct_tbl_len;
        DWORD struct_tbl_addr;
        WORD struct_num;
        BYTE bcd_rev;
    } ep_32;

    struct {
        BYTE ep_anchor[5];
        BYTE ep_chksum;
        BYTE ep_len;
        BYTE major_ver;
        BYTE minor_ver;
        BYTE doc_rev;
        BYTE ep_rev;
        BYTE reserved;
        DWORD max_struct_size;
        QWORD struct_tbl_addr;

    } ep_64;
} __attribute__ ((__packed__)) smbios_ep_t;

/*
 * SMBIOS structure header
 */
typedef struct smbios_struct_hdr {
    BYTE type;
    BYTE len;
    WORD handle;
}  __attribute__ ((__packed__)) smbios_struct_hdr_t;


#define SMBIOS_ENTRY_TAB_LEN sizeof(smbios_ep_t)

/*
 * get smbios structure start address and total len
 * (in) start: pointer of smbios entry table address
 * (out) struct_addr: physical address of smbios structure table
 * (out) struct_len: total length of smbios structure table
 *
 * retrun: 0 for success; non-zero for failures
 */
extern int rsmp_get_smbios_strcut(uint8_t *start, uint64_t *struct_addr, uint16_t *struct_len);

/*
 * get board RMC fingerprint from smbios structure tabe (not entry table)
 * caller should provide mem region of smbios structure table.
 * (in) addr: starting address of structure table in ram (physical or virtual)
 * (out) fp: fingerprint data. Caller must allocate mem.
 *
 * return: retrun: 0 for success; non-zero for failures
 */
extern int rsmp_get_fingerprint_from_smbios_struct(BYTE *addr, rmc_fingerprint_t *fp);

#endif /* INC_RSMP_BIOS_H_ */
