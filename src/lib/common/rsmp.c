/* Copyright (C) 2016 Jianxun Zhang <jianxun.zhang@intel.com>
 *
 * RMC SMBIOS Parser (RSMP)
 */

#include <rsmp.h>

#ifdef RMC_EFI
#include <rmc_util.h>
#endif

/*
 * return a string from given smbios structure table by string's index
 * (in) header: start address of structure table;
 * (in) offset: offset of string defined in SMBIOS spec in the table
 *
 * return: address of string.
 */
static rmc_uint8_t * get_string_from_struct_table(smbios_struct_hdr_t *header, rmc_uint8_t offset){

    rmc_uint8_t str_idx = *((rmc_uint8_t *)header + offset);
    rmc_uint8_t *start = (rmc_uint8_t *)header + header->len;
    rmc_uint8_t i;
    rmc_uint8_t *end = start;

    for (i = 0; i < str_idx; i++) {
        /* search strings in unformatted area, but don't move head if it is what we are looking for */
        for (; *end != '\0'; end++)
            ;

        end++;

        if (i != str_idx - 1)
            start = end;
    }

    return start;
}

/* forward to the starting address of next structure table
 * (in) starting address of current structure table
 *
 *return: starting address of next structure.
 */
static rmc_uint8_t * forward_to_next_struct_table(smbios_struct_hdr_t *header) {

    rmc_uint8_t *str_area = (rmc_uint8_t *)header + header->len;

    for (; !(*str_area == '\0' && *(str_area + 1) == '\0'); str_area++)
        ;

    return (str_area + 2);
}

int rsmp_get_smbios_strcut(rmc_uint8_t *start, rmc_uint64_t *struct_addr, rmc_uint16_t *struct_len){
    smbios_ep_t *ep = (smbios_ep_t *)start;

    /* a 64bit machine can still have 32 bit entry defined by older SMBIOS versions than 3.0,
     * That means we cannot trust architecture but check the signature in entry table.
     */
    if (!strncmp((char *)start, "_SM3_", 5)) {
        *struct_addr = ep->ep_64.struct_tbl_addr;
        *struct_len = ep->ep_64.max_struct_size;
        return 0;
    } else if (!strncmp((char *)start, "_SM_", 4)) {
        *struct_addr = ep->ep_32.struct_tbl_addr;
        *struct_len = ep->ep_32.struct_tbl_len;
        return 0;
    }

    return 1;
}

int rsmp_get_fingerprint_from_smbios_struct(rmc_uint8_t *addr, rmc_fingerprint_t *fp){

    smbios_struct_hdr_t *header = (smbios_struct_hdr_t *)addr;
    int fp_idx;

    if (!fp)
        return 1;

    initialize_fingerprint(fp);

    while (header->type != END_OF_TABLE_TYPE) {
        for (fp_idx = 0; fp_idx < RMC_FINGER_NUM; fp_idx++)
            if (header->type == fp->rmc_fingers[fp_idx].type)
                fp->rmc_fingers[fp_idx].value = (char*)get_string_from_struct_table(header, fp->rmc_fingers[fp_idx].offset);

        header = (smbios_struct_hdr_t *) forward_to_next_struct_table(header);
    }

    return 0;
}
