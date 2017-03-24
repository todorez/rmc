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
 * rmc tool
 *
 *  - Obtain fingerprint of the type of board it runs on
 *  - Generate RMC records and database with fingerprint and board-specific data
 *  - Query file blobs associated to the type of board at run time
 *  - Extract fingerpring and database data
 */

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <rmc_api.h>

#define USAGE "RMC (Runtime Machine configuration) Tool\n" \
    "NOTE: Most of usages require root permission (sudo)\n\n" \
    "rmc -F [-o output_fingerprint]\n" \
    "rmc -R [-f <fingerprint file>] -b <blob file list> [-o output_record]\n" \
    "rmc -D <rmc record file list> [-o output_database]\n" \
    "rmc -B <name of file blob> -d <rmc database file> -o output_file\n\n" \
  "-F: manage fingerprint file\n" \
    "\t-o output_file: store RMC fingerprint of current board in output_file\n" \
  "-R: generate board rmc record of board with its fingerprint and file blobs.\n" \
    "\t-f intput_file : input fingerprint file to be packed in record\n\n" \
    "\tNOTE: RMC will create a fingerprint for the board and use it to\n" \
    "\tgenerate record if an input fingerprint file is not provided.\n\n" \
    "\t-b: files to be packed in record\n\n" \
  "-G: generate rmc database file with records specified in record file list\n\n" \
  "-B: get a file blob with specified name associated to the board rmc is\n" \
  "running on\n" \
    "\t-d: database file to be queried\n" \
    "\t-o: path and name of output file of a specific command\n\n" \
  "-E: Extract data from fingerprint file or database\n" \
    "\t-f: fingerprint file to extract\n" \
    "\t-d: database file to extract\n" \
    "\t-o: directory to extract the database to\n\n" \
    "Examples (Steps in an order to add board support into rmc):\n\n" \
    "1. Generate board fingerprint:\n" \
    "\trmc -F\n\n" \
    "2. Generate a rmc record for the board with two file blobs and save it\n" \
    "to a specified file:\n" \
    "\trmc -R -f fingerprint -b file_1 file_2 -o my_board.record\n\n" \
    "3. Generate a rmc database file with records from 3 different boards:\n" \
    "\trmc -D board1_record board2_record board3_record\n\n" \
    "4. Query a file blob named audio.conf associated to the board rmc is\n" \
    "running on in database my_rmc.db and output to /tmp/new_audio.conf:\n" \
    "\trmc -B audio.conf -d my_rmc.db -o /tmp/new_audio.conf\n\n"


#define RMC_OPT_CAP_F   (1 << 0)
#define RMC_OPT_CAP_R   (1 << 1)
#define RMC_OPT_CAP_D   (1 << 2)
#define RMC_OPT_CAP_B   (1 << 3)
#define RMC_OPT_CAP_E   (1 << 4)
#define RMC_OPT_F       (1 << 5)
#define RMC_OPT_O       (1 << 6)
#define RMC_OPT_B       (1 << 7)
#define RMC_OPT_D       (1 << 8)

static void usage () {
    fprintf(stdout, USAGE);
}

static void dump_fingerprint(rmc_fingerprint_t *fp) {
    int i;

    for (i = 0; i < RMC_FINGER_NUM; i++) {
        printf("Finger %d type   : 0x%02x\n",i, fp->rmc_fingers[i].type);
        printf("Finger %d offset : 0x%02x\n",i, fp->rmc_fingers[i].offset);
        printf("Finger %d name:  : %s\n",i, fp->rmc_fingers[i].name);
        printf("Finger %d value  : %s\n",i, fp->rmc_fingers[i].value);
        printf("\n");
    }
}

/*
 * write fingerprint (in sparse mem) into file
 * (in) pathname        : path and name of file to write.
 * (in) fp              : pointer of fingerprint structure in memory
 *
 * return: 0 for success, non-zero for failures.
 */
static int write_fingerprint_file(const char* pathname, rmc_fingerprint_t *fp) {
    int i;
    int first = 0;
    /* TODO - do we need to open/close file multiple times to write each field */
    for (i = 0; i < RMC_FINGER_NUM; i++) {
        if (write_file(pathname, &fp->rmc_fingers[i].type, sizeof(fp->rmc_fingers[i].type), first))
            return 1;
        /* only first write of first finger should write to the beginning of file */
        first = 1;

        if (write_file(pathname, &fp->rmc_fingers[i].offset, sizeof(fp->rmc_fingers[i].offset), 1) ||
            write_file(pathname, fp->rmc_fingers[i].name, strlen(fp->rmc_fingers[i].name) + 1, 1) ||
            write_file(pathname, fp->rmc_fingers[i].value, strlen(fp->rmc_fingers[i].value) + 1, 1)) {
                fprintf(stderr, "Failed to write fingerprint to %s\n", pathname);
                return 1;
        }
    }

    return 0;
}

typedef enum read_fingerprint_state {
    TYPE = 1,
    OFFSET,
    NAME,
    VALUE
} read_fingerprint_state_t;

/*
 * read fingerprint from file
 * (in) pathname        : path and name of file to read
 * (out) fp             : pointer of fingerprint structure to hold read fingerprint
 * (out) raw            : pointer of a pointer which points to raw file blob allocated.
 *                        caller shall use it to free mem. It is NULL for failures.
 *
 * return: 0 for success, non-zero for failures.
 */
static int read_fingerprint_from_file(const char* pathname, rmc_fingerprint_t *fp, void **raw) {
    char *file = NULL;
    rmc_size_t len = 0;
    rmc_size_t idx = 0;
    int i = 0;
    int ret = 1;
    read_fingerprint_state_t state = TYPE;

    if (read_file(pathname, &file, &len)) {
        fprintf(stderr, "Failed to read fingerprint file: %s\n", pathname);
        return 1;
    }

    /* NOTE: We haven't supported "bring your own SMBIOS fields as fingerprint", that means
     * we still have a hard-coded selected fields when rmc generate a fingerprint for
     * the board. This will be checked here for consistency, because rsmp always seeks same
     * fields at run time on target. It simply gets these fields by calling the same function
     * initialize_fingerprint().
     *
     * The rmcl actually doesn't care which fields are in fingerprint.
     *
     * In the future, we could release the constraint so that user can specify which five fields
     * are the best to describe his board. But we should keep it in mind that same value of two
     * different SMBIOS fields could bring a much higher chance of collision. e.g. a same vendor
     * name in different SMBIOS tables.
     *
     */
    initialize_fingerprint(fp);

    for (idx = 0; idx < len; idx++) {
        switch(state) {
        case TYPE:
            if (fp->rmc_fingers[i].type != file[idx]) {
                fprintf(stderr,
                        "Invalid Finger %d expected type 0x%02x, but got 0x%02x\n\n",
                        i, fp->rmc_fingers[i].type, file[idx]);
                goto read_fp_done;
            }
            state = OFFSET;
            break;
        case OFFSET:
            if (fp->rmc_fingers[i].offset != file[idx]) {
                fprintf(stderr, "Invalid Finger %d expected offset 0x%02x, but got 0x%02x\n\n",
                        i, fp->rmc_fingers[i].offset, file[idx]);
                goto read_fp_done;
            }
            fp->rmc_fingers[i].name = NULL;
            state = NAME;
            break;
        case NAME:
            if (fp->rmc_fingers[i].name == NULL)
                fp->rmc_fingers[i].name = &file[idx];
            if (file[idx] == '\0') {
                fp->rmc_fingers[i].value = NULL;
                state = VALUE;
            }
            break;
        case VALUE:
            if (fp->rmc_fingers[i].value == NULL)
                fp->rmc_fingers[i].value = &file[idx];
            if (file[idx] == '\0') {
                /* next fingerprint */
                i++;

                if (!(i < RMC_FINGER_NUM)) {
                    /* we have got all fingers' data, done */
                    ret = 0;
                    goto read_fp_done;
                }
                state = TYPE;
            }
            break;
        default:
            fprintf(stderr, "Internal error, invalid state when parsing fingerprint\n\n");
            goto read_fp_done;
        }
    }
read_fp_done:
    if (ret) {
        if (i != RMC_FINGER_NUM)
            fprintf(stderr, "Internal error when parsing finger %d. file could be corrupted\n\n", i);

        free(file);
        *raw = NULL;
    } else
        *raw = file;

    return ret;
}

/*
 * Read a file blob into rmc file structure
 * (in) pathname        : path and name of file
 * (in) type            : policy type that must be RMC_GENERIC_FILE
 *
 * return               : a pointer to rmc file structure. Caller shall
 *                        free memory for returned data AND blob
 *                        Null is returned for failures.
 */
static rmc_file_t *read_policy_file(char *pathname, int type) {
    rmc_file_t *tmp = NULL;
    rmc_size_t policy_len = 0;
    char *path_token;

    if ((tmp = calloc(1, sizeof(rmc_file_t))) == NULL) {
        fprintf(stderr, "Failed to allocate memory for file blob\n\n");
        return NULL;
    }

    tmp->type = type;
    tmp->next = NULL;

    if (type == RMC_GENERIC_FILE) {
        if (read_file(pathname, (char **)&tmp->blob, &policy_len)) {
            fprintf(stderr, "Failed to read file %s\n\n", pathname);
            free(tmp);
            return NULL;
        }
        tmp->blob_len = policy_len;
        path_token = strrchr(pathname, '/');
        if (!path_token)
            tmp->blob_name = strdup(pathname);
        else
            tmp->blob_name = strdup(path_token + 1);

        if (!tmp->blob_name) {
            fprintf(stderr, "Failed to allocate mem for policy file name %s\n\n", pathname);
            free(tmp->blob);
            free(tmp);
            return NULL;
        }
    } else {
        fprintf(stderr, "Failed to read policy file %s, unknown type %d\n\n", pathname, type);
        free(tmp);
        return NULL;
    }

    return tmp;
}

/*
 * Read a record file into record file structure
 * (in) pathname        : path and name of record file
 *
 * return               : a pointer to record file structure. Caller shall
 *                        free memory for returned data AND its blobs
 *                        Null is returned for failures.
 */
static rmc_record_file_t *read_record_file(char *pathname) {

    rmc_record_file_t *tmp = NULL;
    int ret;

    if ((tmp = calloc(1, sizeof(rmc_record_file_t))) == NULL) {
        fprintf(stderr, "Failed to allocate memory for record file\n\n");
        return NULL;
    }

    ret = read_file(pathname, (char **)&tmp->blob, &tmp->length);
            if (ret) {
                fprintf(stderr, "Failed to read record file %s\n\n", pathname);
                free(tmp);
                return NULL;
            }

    return tmp;
}

int main(int argc, char **argv){

    int c;
    rmc_uint16_t options = 0;
    char *output_path = NULL;
    char *input_db_path_d = NULL;
    char **input_file_blobs = NULL;
    char **input_record_files = NULL;
    char *input_fingerprint = NULL;
    char *input_blob_name = NULL;
    rmc_fingerprint_t fingerprint;
    rmc_file_t *policy_files = NULL;
    rmc_record_file_t *record_files = NULL;
    void *raw_fp = NULL;
    rmc_uint8_t *db = NULL;
    rmc_uint8_t *db_c = NULL;
    rmc_uint8_t *db_d = NULL;
    int ret = 1;
    int i;
    int arg_num = 0;

    if (argc < 2) {
        usage();
        return 0;
    }

    /* parse options */
    opterr = 0;

    while ((c = getopt(argc, argv, "FRED:B:b:f:o:d:")) != -1)
        switch (c) {
        case 'F':
            options |= RMC_OPT_CAP_F;
            break;
        case 'R':
            options |= RMC_OPT_CAP_R;
            break;
        case 'E':
            options |= RMC_OPT_CAP_E;
            break;
        case 'D':
            /* we don't know number of arguments for this option at this point,
             * allocate array with argc which is bigger than needed. But we also
             * need one extra for terminator element.
             */
            input_record_files = calloc(argc, sizeof(char *));

            if (!input_record_files) {
                fprintf(stderr, "No enough mem to process `-%c'.\n\n", optopt);
                /* ! must exit program, memory allocated by other options not freed */
                exit(1);
            }

            optind--;
            arg_num = 0;

            while (optind < argc && argv[optind][0] != '-') {
                if ((input_record_files[arg_num++] = strdup(argv[optind++])) == NULL) {
                    perror("rmc: cannot allocate mem for arguments of -D");
                    exit(1);
                }
            }

            options |= RMC_OPT_CAP_D;
            break;
        case 'B':
            input_blob_name = optarg;
            options |= RMC_OPT_CAP_B;
            break;
        case 'o':
            output_path = optarg;
            options |= RMC_OPT_O;
            break;
        case 'f':
            input_fingerprint = optarg;
            options |= RMC_OPT_F;
            break;
        case 'd':
            input_db_path_d = optarg;
            options |= RMC_OPT_D;
            break;
        case 'b':
            /* we don't know nubmer of arguments for this option at this point,
             * allocate array with argc which is bigger than needed. But we also
             * need one extra for terminator element.
             */
            input_file_blobs = calloc(argc, sizeof(char *));

            if (!input_file_blobs) {
                fprintf(stderr, "No enough mem to process `-%c'.\n\n", optopt);
                /* ! must exit program, memory allocated by other options not freed */
                exit(1);
            }

            optind--;
            arg_num = 0;

            while (optind < argc && argv[optind][0] != '-') {
                if ((input_file_blobs[arg_num++] = strdup(argv[optind++])) == NULL) {
                    perror("rmc: cannot allocate mem for arguments of -b");
                    exit(1);
                }
            }

            options |= RMC_OPT_B;

            break;
        case '?':
            if (optopt == 'F' || optopt == 'R' || optopt == 'D' || optopt == 'B' || \
                    optopt == 'E' ||  optopt == 'b' || optopt == 'f' || \
                    optopt == 'o' || optopt == 'd')
                fprintf(stderr, "\nWRONG USAGE: -%c\n\n", optopt);
            else if (isprint(optopt))
                fprintf(stderr, "Unknown option `-%c'.\n\n", optopt);
            else
                fprintf(stderr, "Unknown option character `\\x%x'.\n\n", optopt);
            usage();
            return 1;
        default:
            return 1;
        }

    /* sanity check for -o */
    if (options & RMC_OPT_O) {
        rmc_uint16_t opt_o = options & (RMC_OPT_CAP_D | RMC_OPT_CAP_R |
            RMC_OPT_CAP_F | RMC_OPT_CAP_B | RMC_OPT_CAP_E);
        if (!(opt_o)) {
            fprintf(stderr, "\nWRONG: Option -o cannot be applied without -B, -D, -E, -R or -F\n\n");
            usage();
            return 1;
        } else if (opt_o != RMC_OPT_CAP_D && opt_o != RMC_OPT_CAP_R &&
            opt_o != RMC_OPT_CAP_F && opt_o != RMC_OPT_CAP_B  && opt_o != RMC_OPT_CAP_E) {
            fprintf(stderr, "\nWRONG: Option -o can be applied with only one of -B, -D, -R and -F\n\n");
            usage();
            return 1;
        }
    }

    /* sanity check for -R */
    if ((options & RMC_OPT_CAP_R) && (!(options & RMC_OPT_B))) {
        fprintf(stderr, "\nWRONG: -b is required when -R is present\n\n");
        usage();
        return 1;
    }

    /* sanity check for -E */
    if ((options & RMC_OPT_CAP_E) && (!(options & RMC_OPT_F) && !(options & RMC_OPT_D))) {
        fprintf(stderr, "\nERROR: -E requires -f <fingerprint file name> or -d <database file name>\n\n");
        usage();
        return 1;
    }

    /* sanity check for -B */
    if ((options & RMC_OPT_CAP_B) && (!(options & RMC_OPT_D) || !(options & RMC_OPT_O))) {
        fprintf(stderr, "\nWRONG: -B requires -d and -o\n\n");
        usage();
        return 1;
    }

    /* get a file blob */
    if (options & RMC_OPT_CAP_B) {
        rmc_file_t file;

        if (!output_path) {
            fprintf(stderr, "-B internal error, with -o but no output \
                pathname specified\n\n");
            goto main_free;
        }

        if (rmc_gimme_file(input_db_path_d, input_blob_name, &file))
            goto main_free;

        if (write_file(output_path, file.blob, file.blob_len, 0)) {
            fprintf(stderr, "-B failed to write file %s to %s\n\n",
                input_blob_name, output_path);
            rmc_free_file(&file);
            goto main_free;
        }
        rmc_free_file(&file);
    }

    if (options & RMC_OPT_CAP_E) {
        /* print fingerpring file to console*/
        if (options & RMC_OPT_F) {
            rmc_fingerprint_t fp;
            /* read fingerprint file*/
            if (input_fingerprint != NULL) {
                if (read_fingerprint_from_file(input_fingerprint, &fp, &raw_fp)) {
                    fprintf(stderr, "Cannot read fingerprint from %s\n\n",
                    input_fingerprint);
                    goto main_free;
                }
                dump_fingerprint(&fp);
            }else {
                printf("Fingerprint file not provided! Exiting.\n");
            }
        } else if (options & RMC_OPT_D) {
            if(dump_db(input_db_path_d, output_path)) {
               fprintf(stderr, "\nFailed to extract %s\n\n", input_db_path_d);
               goto main_free;
            } else
               printf("\nSuccessfully extracted %s\n\n", input_db_path_d);
        }
    }

    /* generate RMC database file */
    if (options & RMC_OPT_CAP_D) {
        int record_idx = 0;
        rmc_record_file_t *record = NULL;
        rmc_record_file_t *current_record = NULL;
        rmc_size_t db_len = 0;

        /* if user doesn't provide pathname for output database, set a default value */
        if (output_path == NULL)
            output_path = "rmc.db";
        /* read record files into a list */
        while (input_record_files && input_record_files[record_idx]) {
            char *s = input_record_files[record_idx];

            if ((record = read_record_file(s)) == NULL) {
                fprintf(stderr, "Failed to read record file %s\n\n", s);
                goto main_free;
            }

            if(!record_files) { /* 1st iteration */
                record_files = record;
                current_record = record;
            } else {
                current_record->next = record;
                current_record = current_record->next;
            }

            record_idx++;
        }

        /* call rmcl to generate DB blob */
        if (rmcl_generate_db(record_files, &db, &db_len)) {
            fprintf(stderr, "Failed to generate database blob\n\n");
            goto main_free;
        }

        /* write rmc database file */
        if (write_file(output_path, db, db_len, 0)) {
            fprintf(stderr, "Failed to write RMC database to %s\n\n", output_path);
            goto main_free;
        }
    }

    /* generate RMC record file with a list of file blobs. */
    if (options & RMC_OPT_CAP_R) {
        rmc_fingerprint_t fp;
        rmc_fingerprint_t *free_fp = NULL;
        rmc_file_t *policy = NULL;
        rmc_file_t *current_policy = NULL;
        rmc_record_file_t record;
        int policy_idx = 0;

        /* if user doesn't provide pathname for output record, set a default value */
        if (output_path == NULL)
            output_path = "rmc.record";

        /* read fingerprint file*/
        if (input_fingerprint != NULL) {
            if (read_fingerprint_from_file(input_fingerprint, &fp, &raw_fp)) {
                fprintf(stderr, "Cannot read fingerprint from %s\n\n",
                        input_fingerprint);
                goto main_free;
            }

            /* for debug */
            printf("Successfully got fingerprint from %s \n", input_fingerprint);
            dump_fingerprint(&fp);
        }else {
            printf("Fingerprint file not provided, generate one for board we are running\n");
            if (rmc_get_fingerprint(&fp)) {
                fprintf(stderr, "Failed to generate fingerprint for this board\n\n");
                goto main_free;
            } else
                free_fp = &fp;
        }

        /* read policy file into a list */
        while (input_file_blobs && input_file_blobs[policy_idx]) {
            char *s = input_file_blobs[policy_idx];

            if ((policy = read_policy_file(s, RMC_GENERIC_FILE)) == NULL) {
                fprintf(stderr, "Failed to read policy file %s\n\n", s);
                rmc_free_fingerprint(free_fp);
                goto main_free;

            }

            if (!policy_files) { /* 1st iteration */
                policy_files = policy;
                current_policy = policy;
            } else {
                current_policy->next = policy;
                current_policy = current_policy->next;
            }
            policy_idx++;
        }

        /* call rmcl to generate record blob */
        if (rmcl_generate_record(&fp, policy_files, &record)) {
            fprintf(stderr, "Failed to generate record for this board\n\n");
            rmc_free_fingerprint(free_fp);
            goto main_free;
        }

        rmc_free_fingerprint(free_fp);

        /* write record blob into file*/
        if (write_file(output_path, record.blob, record.length, 0)) {
            fprintf(stderr, "Failed to write record to %s\n\n", output_path);
            goto main_free;
        }
    }

    if (options & RMC_OPT_CAP_F) {
        /* set a default fingerprint file name if user didn't provide one */
        if (!output_path)
            output_path = "rmc.fingerprint";

        if (rmc_get_fingerprint(&fingerprint)) {
            fprintf(stderr, "Cannot get board fingerprint\n");
            goto main_free;
        }

        printf("Got Fingerprint for board:\n\n");
        dump_fingerprint(&fingerprint);

        if (write_fingerprint_file(output_path, &fingerprint)) {
            fprintf(stderr, "Cannot write board fingerprint to %s\n", output_path);
            rmc_free_fingerprint(&fingerprint);
            goto main_free;
        }
        rmc_free_fingerprint(&fingerprint);
    }

    ret = 0;

main_free:

    free(db_c);

    free(db_d);

    i = 0;
    if (input_file_blobs) {
        while (input_file_blobs[i] != NULL) {
            free(input_file_blobs[i]);
            i++;
        }

        free(input_file_blobs);
    }

    i = 0;

    if (input_record_files) {
        while (input_record_files[i] != NULL) {
            free(input_record_files[i]);
            i++;
        }

        free(input_record_files);
    }

    while (policy_files) {
        rmc_file_t *t = policy_files;
        policy_files = policy_files->next;
        free(t->blob);
        free(t->blob_name);
        free(t);
    }

    while (record_files) {
        rmc_record_file_t *t = record_files;
        record_files = record_files->next;
        free(t->blob);
        free(t);
    }

    free(raw_fp);
    free(db);

    return ret;
}
