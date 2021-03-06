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

/* RMC Library */

#include <rmc_types.h>
#include <rmcl.h>

#ifdef RMC_EFI
#include <rmc_util.h>
#endif

static const rmc_uint8_t rmc_db_signature[RMC_DB_SIG_LEN] = {'R', 'M', 'C', 'D', 'B'};

/* compute a finger to signature which is stored in record
 * (in) fingerprint : of board, usually generated by rmc tool and rsmp
 * (out) signature  : fixed-length unique data as the final identifier of board
 *
 * return: 0 for success, or non-zero for failures
 */
static int generate_signature_from_fingerprint(rmc_fingerprint_t *fingerprint, rmc_signature_t *signature) {

    rmc_size_t sig_len = sizeof(signature->raw);
    rmc_size_t i = 0;
    rmc_size_t p = 0;
    rmc_size_t q = 0;

    if (!signature || !fingerprint)
        return 1;

    /* We will have more advanced hash algorithm */
    memset(signature->raw, 0, sig_len);

    /* pick a byte from each finger ever time. Interleaving is just to provide a fair chance
     * for all involved finger values without an arbitrary quota, not for hash-like purposes */
    for (i = 0; i < sig_len; i++) {
        if (i & 1) {
            /* When reach the end of either of two finger values, don't copy '\0' which is a common
             * character in every string. By doing so we have 2 more bytes for valuable info in
             * signature.
             * In the same case, take whatever left in another finger's data for the rest of signature.
             */
            if (fingerprint->named_fingers.index.value[p] == '\0') {
                strncpy((char *)(signature->raw + i), fingerprint->named_fingers.thumb.value + q, sig_len - i);
                break;
            }
            else
                signature->raw[i] = fingerprint->named_fingers.index.value[p++];
        } else {
            if (fingerprint->named_fingers.thumb.value[q] == '\0') {
                strncpy((char *)(signature->raw + i), fingerprint->named_fingers.index.value + p, sig_len - i);
                break;
            }
            else
                signature->raw[i] = fingerprint->named_fingers.thumb.value[q++];
        }
    }
    return 0;
}

#ifndef RMC_EFI
int rmcl_generate_record(rmc_fingerprint_t *fingerprint, rmc_file_t *policy_files, rmc_record_file_t *record_file) {

    rmc_file_t *tmp = NULL;
    rmc_uint64_t cmd_len = 0;
    rmc_size_t record_len = 0;
    rmc_uint8_t *blob = NULL;
    rmc_uint8_t *idx = NULL;
    rmc_record_header_t *record = NULL;
    rmc_meta_header_t *meta = NULL;

    if (!record_file || !fingerprint || !policy_files)
        return 1;

    tmp = policy_files;
    record_len = sizeof(rmc_record_header_t);

    /* Calculate total length of record for memory allocation */
    while (tmp) {
        record_len += sizeof(rmc_meta_header_t) + strlen(tmp->blob_name) + 1;
        record_len += tmp->blob_len;
        tmp = tmp->next;
    }

    blob = malloc(record_len);

    if (!blob)
        return 1;

    /* generate signature from fingerprint */
    record = (rmc_record_header_t *)blob;

    if (generate_signature_from_fingerprint(fingerprint, &record->signature)) {
        free(blob);
        return 1;
    }

    record->length = record_len;

    /* pack all policy files into record blob */
    /* TODO: Change to human-readable format) */
    idx = blob + sizeof(rmc_record_header_t);

    tmp = policy_files;

    while (tmp) {
        meta = (rmc_meta_header_t *)idx;
        meta->type = tmp->type;
        meta->length = sizeof(rmc_meta_header_t);
        idx += sizeof(rmc_meta_header_t);

        cmd_len = strlen(tmp->blob_name) + 1;

        memcpy(idx, tmp->blob_name, cmd_len);
        idx += cmd_len;
        meta->length += cmd_len;
        memcpy(idx, tmp->blob, tmp->blob_len);
        idx += tmp->blob_len;
        meta->length += tmp->blob_len;

        tmp = tmp->next;
    }

    record_file->length = record_len;
    record_file->next = NULL;
    record_file->blob = blob;

    return 0;
}

int rmcl_generate_db(rmc_record_file_t *record_files, rmc_uint8_t **rmc_db, rmc_size_t *len) {

    rmc_record_file_t *tmp = NULL;
    rmc_uint64_t db_len = sizeof(rmc_db_header_t);
    rmc_db_header_t *db = NULL;
    rmc_uint8_t *idx = NULL;
    int i;

    if (!record_files || !len)
        return 1;

    if (rmc_db)
        *rmc_db = NULL;
    else
        return 1;

    *len = 0;
    tmp = record_files;

    /* Calculate total length of database for memory allocation */
    while (tmp) {
        db_len += tmp->length;
        tmp = tmp->next;
    }

    db = malloc(db_len);

    if (!db)
        return 1;

    /* set DB signature*/
    for (i = 0; i < RMC_DB_SIG_LEN; i++)
        db->signature[i] = rmc_db_signature[i];

    db->version = 0x1;

    db->length = db_len;
    idx = (rmc_uint8_t *)db;
    idx += sizeof(rmc_db_header_t);

    tmp = record_files;

    /* pack all records into db blob */
    while (tmp) {
        memcpy(idx, tmp->blob, tmp->length);
        idx += tmp->length;
        tmp = tmp->next;
    }

    *rmc_db = (rmc_uint8_t *)db;
    *len = db_len;

    return 0;
}

#endif /* RMC_EFI */
/*
 * Check if a record has signature matched with a given signature
 *
 * return 0 if record has matched signature or non-zero in other cases
 */
static int match_record(rmc_record_header_t *r, rmc_signature_t* sig) {
    return strncmp((const char *)r->signature.raw, (const char *)sig->raw, sizeof(r->signature.raw));
}

int is_rmcdb(rmc_uint8_t *db_blob) {
    rmc_db_header_t *db_header = NULL;

    if (db_blob == NULL)
        return 1;

    db_header = (rmc_db_header_t *)db_blob;

    /* sanity check of db */
    if (strncmp((const char *)db_header->signature, (const char *)rmc_db_signature, RMC_DB_SIG_LEN))
        return 1;
    else
        return 0;
}

int query_policy_from_db(rmc_fingerprint_t *fingerprint, rmc_uint8_t *rmc_db, rmc_uint8_t type, char *blob_name, rmc_file_t *policy) {
    rmc_meta_header_t meta_header;
    rmc_db_header_t *db_header = NULL;
    rmc_record_header_t record_header;
    rmc_signature_t signature;
    rmc_uint64_t record_idx = 0;   /* offset of each reacord in db*/
    rmc_uint64_t meta_idx = 0;     /* offset of each meta in a record */
    rmc_uint64_t policy_idx = 0;   /* offset of policy in a meta */

    if (!fingerprint || !rmc_db || !policy)
        return 1;

    if (type != RMC_GENERIC_FILE || blob_name == NULL)
        return 1;

    db_header = (rmc_db_header_t *)rmc_db;

    /* sanity check of db */
    if (is_rmcdb(rmc_db))
        return 1;

    /* calculate signature of fingerprint */
    if(generate_signature_from_fingerprint(fingerprint, &signature))
        return 1;

    /* query the meta. idx: start of record */
    for (record_idx = sizeof(rmc_db_header_t); record_idx < db_header->length;) {
        /* record data may not be aligned like db pointer does
         * We align record header by copying
         */
        memcpy(&record_header, rmc_db + record_idx, sizeof(rmc_record_header_t));

        /* found matched record */
        if (!match_record(&record_header, &signature)) {
            /* find meta by type and name */
            for (meta_idx = record_idx + sizeof(rmc_record_header_t); meta_idx < record_idx + record_header.length;) {
                 /* re-align meta header struct by copying*/
                memcpy(&meta_header, rmc_db + meta_idx, sizeof(rmc_meta_header_t));

                if (meta_header.type == type) {

                    policy_idx = meta_idx + sizeof(rmc_meta_header_t);

                    if (!strncmp(blob_name, (char *)&rmc_db[policy_idx], strlen(blob_name) + 1)) {
                        rmc_ssize_t cmd_name_len = strlen((char *)&rmc_db[policy_idx]) + 1;
                        policy->blob = &rmc_db[policy_idx + cmd_name_len];
                        policy->blob_len = meta_header.length - sizeof(rmc_meta_header_t) - cmd_name_len;
                        policy->next = NULL;
                        policy->type = type;

                        return 0;
                    }
                }

                meta_idx += meta_header.length;
            } /* traverse in record */
        }

        record_idx += record_header.length;
    } /* traverse in db */

    return 1;
}
