/* Copyright (C) 2016 Jianxun Zhang <jianxun.zhang@intel.com>
 *
 * header file for standard smbios structure definitions
 * Reference: SMBIOS Reference Specification 3.0.0
 */
#ifndef INC_RSMP_H_
#define INC_RSMP_H_

#include <rmcl.h>

#pragma pack(push, 1)

/*
 * SMBIOS Entry Point Structure
 */

typedef union smbios_ep {
    /* */
    struct {
        rmc_uint8_t ep_anchor[4];
        rmc_uint8_t ep_chksum;
        rmc_uint8_t ep_len;
        rmc_uint8_t major_ver;
        rmc_uint8_t minor_ver;
        rmc_uint16_t max_struct_size;
        rmc_uint8_t ep_rev;
        rmc_uint8_t fmt_area[5];
        rmc_uint8_t interm_anchor[5];
        rmc_uint8_t interm_chksum;
        rmc_uint16_t struct_tbl_len;
        rmc_uint32_t struct_tbl_addr;
        rmc_uint16_t struct_num;
        rmc_uint8_t bcd_rev;
    } ep_32;

    struct {
        rmc_uint8_t ep_anchor[5];
        rmc_uint8_t ep_chksum;
        rmc_uint8_t ep_len;
        rmc_uint8_t major_ver;
        rmc_uint8_t minor_ver;
        rmc_uint8_t doc_rev;
        rmc_uint8_t ep_rev;
        rmc_uint8_t reserved;
        rmc_uint32_t max_struct_size;
        rmc_uint64_t struct_tbl_addr;

    } ep_64;
} __attribute__ ((__packed__)) smbios_ep_t;

/*
 * SMBIOS structure header
 */
typedef struct smbios_struct_hdr {
    rmc_uint8_t type;
    rmc_uint8_t len;
    rmc_uint16_t handle;
}  __attribute__ ((__packed__)) smbios_struct_hdr_t;


#define SMBIOS_ENTRY_TAB_LEN sizeof(smbios_ep_t)

#pragma pack(pop)

/*
 * get smbios structure start address and total len
 * (in) start: pointer of smbios entry table address
 * (out) struct_addr: physical address of smbios structure table
 * (out) struct_len: total length of smbios structure table
 *
 * retrun: 0 for success; non-zero for failures
 */
extern int rsmp_get_smbios_strcut(rmc_uint8_t *start, rmc_uint64_t *struct_addr, rmc_uint16_t *struct_len);

/*
 * get board RMC fingerprint from smbios structure tabe (not entry table)
 * caller should provide mem region of smbios structure table.
 * (in) addr: starting address of structure table in ram (physical or virtual)
 * (out) fp: fingerprint data. Caller must allocate mem.
 *
 * return: retrun: 0 for success; non-zero for failures
 */
extern int rsmp_get_fingerprint_from_smbios_struct(rmc_uint8_t *addr, rmc_fingerprint_t *fp);

#endif /* INC_RSMP_H_ */
