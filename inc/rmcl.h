/* Copyright (C) 2016 Jianxun Zhang <jianxun.zhang@intel.com>
 *
 * RMC Library (RMCL)
 *
 * Provide API and RMC data types of RMCL to outside
 */

#ifndef INC_RMCL_H_
#define INC_RMCL_H_
#include <stdint.h>
#include <stddef.h>
#include <rmc_types.h>

#pragma pack(1)

/*
 * RMC Fingerprint
 */

#define END_OF_TABLE_TYPE 127

/*
 * structures for RMC fingerprint
 * INIT_RMC_FINGERPRINTS macro is
 * preferred as long as possible.
 */
typedef struct rmc_finger {
    BYTE type;
    BYTE offset;
    char *name;
    char *value;
} rmc_finger_t;

#define RMC_FINGER_NUM (5)

typedef union rmc_fingerprint {
    rmc_finger_t rmc_fingers[RMC_FINGER_NUM];
    struct rmc_finger_by_name {
        rmc_finger_t thumb;
        rmc_finger_t index;
        rmc_finger_t middle;
        rmc_finger_t ring;
        rmc_finger_t pink;
    } named_fingers;
} rmc_fingerprint_t;

/*
 * initialize fingerprint data
 */

static __inline__ void initialize_fingerprint(rmc_fingerprint_t *fp) {
    fp->named_fingers.thumb.type = 0x1;
    fp->named_fingers.thumb.offset = 0x5;
    fp->named_fingers.thumb.name = "product_name";
    fp->named_fingers.thumb.value = "";
    fp->named_fingers.index.type = 0x2;
    fp->named_fingers.index.offset = 0x5;
    fp->named_fingers.index.name = "product_name";
    fp->named_fingers.index.value = "";
    fp->named_fingers.middle.type = 0x4;
    fp->named_fingers.middle.offset = 0x10;
    fp->named_fingers.middle.name = "version";
    fp->named_fingers.middle.value = "";
    fp->named_fingers.ring.type = 0x7f;
    fp->named_fingers.ring.offset = 0x0;
    fp->named_fingers.ring.name = "reserved";
    fp->named_fingers.ring.value = "";
    fp->named_fingers.pink.type = 0x7f;
    fp->named_fingers.pink.offset = 0x0;
    fp->named_fingers.pink.name = "reserved";
    fp->named_fingers.pink.value = "";
}

#define RMC_DB_SIG_LEN 5
/*
 * RMC Database (packed). A RMC DB contains records
 */
typedef struct rmc_db_header {
    BYTE signature[RMC_DB_SIG_LEN];
    BYTE version;
    QWORD length;
} __attribute__ ((__packed__)) rmc_db_header_t;

/* signature is the computation result of fingerprint and what's packed in record */
typedef union rmc_signature {
    BYTE raw[32];
} __attribute__ ((__packed__)) rmc_signature_t;

/*
 * RMC Database Record (packed). A record contains metas for a board type
 */
typedef struct rmc_record_header {
    rmc_signature_t signature;     /* computation result from finger print */
    QWORD length;
} __attribute__ ((__packed__)) rmc_record_header_t;

/*
 * RMC Database Meta (packed)
 */
typedef struct rmc_meta_header {
    BYTE type;      /* type 0 command line; type 1 file blob*/
    QWORD length;   /* covers cmdline_filename and blob blocks. */
    /* char *cmdline_filename : Invisible, null-terminated string packed in mem */
    /* BYTE *blob : Invisible, binary packed in mem */
} __attribute__ ((__packed__)) rmc_meta_header_t;

/*
 * input of a rmc policy file. RMCL accepts command line
 * and file blobs presented in this data type (sparse memory)
 */
#define RMC_POLICY_CMDLINE 0
#define RMC_POLICY_BLOB 1
typedef struct rmc_policy_file {
    BYTE type;              /* RMC_POLICY_CMDLINE or RMC_POLICY_BLOB*/
    char *cmdline_name;     /* file name of blob (type 1) or command line fragment (type 0) */
    struct rmc_policy_file *next;  /* next rmc policy file, or null as terminator for the last element */
    size_t blob_len;         /* number of bytes of blob, excluding length of name */
    BYTE *blob;             /* blob of policy file, treated as binary, UNNECESSARILY Null terminated */
} rmc_policy_file_t;

/*
 * output of a rmc record file generated by rmcl.
 * Also as input when generating rmc db with records.
 */
typedef struct rmc_record_file {
    BYTE *blob;              /* record raw data blob */
    size_t length;
    struct rmc_record_file *next;  /* next rmc record file, or null as terminator for the last element */
} rmc_record_file_t;

#ifdef RMC_UEFI_CONTEXT
/* To be implemented */
static __inline__ BYTE * rmc_malloc(size_t size) {
    return NULL;
}

static __inline__ void rmc_free(void * p) {

}

#else /* linux user space */
#include <stdlib.h>

static __inline__ void *rmc_malloc(size_t size) {
    return malloc(size);
}

static __inline__ void rmc_free(void * p) {
    free(p);
}

#endif

/*
 * Generate RMC record file (This function allocate memory)
 * (in) fingerprint     : fingerprint of board, usually generated by rmc tool with rsmp.
 * (in) policy files    : head of a list of policy files, 'next' of the last one must be NULL.
 * (out) rmc_record     : generated rmc record blob with its length
 * (ret) 0 for success, RMC error code for failures. content of rmc record is undefined for failure.
 */
extern int rmcl_generate_record(rmc_fingerprint_t *fingerprint, rmc_policy_file_t *policy_files, rmc_record_file_t *record_file);

/*
 * Generate RMC database blob (This function allocate memory)
 * (in) record_files    : head of a list of record files, 'next' of the last one must be NULL.
 * (in) file_num        : number of rmc files passed in
 * (out) rmc_db         : generated rmc database blob, formated by rmcl.
 * (out) len            : length of returned rmc db
 * (ret) 0 for success, RMC error code for failures. content of rmc_db is NULL for failures.
 */
extern int rmcl_generate_db(rmc_record_file_t *record_files, BYTE **rmc_db, size_t *len);

/*
 * Query a RMC database blob provided by caller, to get kernel command line fragment or a policy file blob.
 * (in) fingerprint     : fingerprint of board
 * (in) rmc_db          : rmc database blob
 * (in) type            : type of record
 * (in) blob_name       : name of file blob to query. Ignored when type is 0 (cmdline)
 * (out) policy         : policy file data structure provided by caller, holding returned policy data
 *                        if there is a matched meta for board. Pointer members of policy hold data location
 *                        in rmc_db's memory region. (This functions doesn't copy data.)
 *
 * return               : 0 when rmcl found a meta in record which has matched signature of fingerprint. non-zero for failures. Content of
 *                        policy is not determined when non-zero is returned.
 */
extern int query_policy_from_db(rmc_fingerprint_t *fingerprint, BYTE *rmc_db, BYTE type, char *blob_name, rmc_policy_file_t *policy);

#endif /* INC_RMCL_H_ */
