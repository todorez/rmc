/*
 * rmc tool
 *
 * An executable supports:
 *  - Provide fingerprint in human-readable RMC fragment (header) file
 *  - Generate RMC database file from human-readable RMC fragment files
 *  - Provide policy fragments like command line to its callers in user space
 */

#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <rmcl.h>
#include <rsmp.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>


#define USAGE "RMC (Runtime Machine configuration) Tool\n" \
    "NOTE: Most of usages require root permission (sudo)\n" \
    "rmc -F [-o output_fingerprint]\n" \
    "rmc -R [-f <fingerprint file>] -c <cmdline file> | -b <blob file list> [-o output_record]\n" \
    "rmc -D <rmc record file list> [-o output_database]\n" \
	"rmc -C <rmc database file>\n" \
	"rmc -B <name of file blob> -d <rmc database file> -o output_file\n" \
	"\n" \
	"-F: generate board rmc fingerprint of board\n" \
	"-R: generate board rmc record of board with its fingerprint, kenrel commandline and blob files.\n" \
    "-f: fingerprint file to be packed in record, rmc will create a fingerprint for board and use it internally to\n" \
    "    generate record if -f is missed.\n" \
    "-c: kernel command line fragment to be packed in record\n" \
    "-b: files to be packed in record\n" \
	"Note: At least one of -f and -c must be provided when -R is present\n" \
	"-G: generate rmc database file with records specified in record file list\n" \
	"-C: get kernel command line fragment from database file for the board rmc is running on, output to stdout\n" \
	"-B: get a flie blob with specified name associated to the board rmc is running on\n" \
	"-d: database file to be queried\n" \
	"-o: path and name of output file of a specific command\n" \
	"\n" \
    "Examples (Steps in an order to add board support into rmc):\n" \
    "generate board fingerprint:\n" \
    "rmc -F\n\n" \
    "generate a rmc record for the board with a kernel command line fragment and two files, output to:\n" \
    "a specified file:\n" \
    "rmc -R -f fingerprint -c my_cmdline -b file_1 file_2 -o my_board.record\n\n" \
    "generate a rmc database file with records from 3 different boards:\n" \
    "rmc -D board1_record board2_record board3_record\n\n" \
    "output command line fragment for the board rmc is running on, from a rmc database mn_rmc.db:\n" \
    "rmc -C my_rmc.db\n\n" \
    "query a file blob named audio.conf associated to the board rmc is running on in database my_rmc.db and output\n" \
    "to /tmp/new_audio.conf:\n" \
    "rmc -B audio.conf -d my_rmc.db -o /tmp/new_audio.conf\n\n"


#define RMC_OPT_CAP_F   (1 << 0)
#define RMC_OPT_CAP_R   (1 << 1)
#define RMC_OPT_CAP_D   (1 << 2)
#define RMC_OPT_CAP_C   (1 << 3)
#define RMC_OPT_CAP_B   (1 << 4)
#define RMC_OPT_C       (1 << 5)
#define RMC_OPT_F       (1 << 6)
#define RMC_OPT_O       (1 << 7)
#define RMC_OPT_B       (1 << 8)
#define RMC_OPT_D       (1 << 9)

#define EFI_SYSTAB_PATH "/sys/firmware/efi/systab"
#define SYSTAB_LEN 4096         /* assume 4kb is enough...*/


/*
 * utility function to read a file into mem. This function allocates memory
 * (in)  pathname   : file pathname to read
 * (out) data       : address of point that points to the data read
 * (out) len        : pointer of total number of bytes read from file
 *
 * return           : 0 for success, non-zeor for failures
 */
static int read_file(const char *pathname, char **data, size_t* len) {
    int fd = -1;
    struct stat s;
    off_t total = 0;
    void *buf = NULL;
    size_t byte = 0;
    ssize_t tmp = 0;

    *data = NULL;
    *len = 0;

    if (stat(pathname, &s) < 0) {
        perror("rmc: failed to get file stat");
        return 1;
    }

    total = s.st_size;

    if ((fd = open(pathname, O_RDONLY)) < 0) {
        perror("rmc: failed to open file to read");
        return 1;
    }

    buf = malloc(total);

    if (!buf) {
        perror("rmc: failed to alloc read buf");
        return 1;
    }

    while (byte < total) {
        if ((tmp = read(fd, buf + byte, total - byte)) < 0) {
            perror("rmc: failed to alloc read buf");
            free(buf);
            close(fd);
            return 1;
        }

        byte += (size_t)tmp;
    }

    *data = buf;
    *len = byte;

    close(fd);
    return 0;
}

/*
 * utility function to write data into file.
 * (in) pathname   : file pathname to write
 * (in) data       : pointer of data buffer
 * (in) len        : total number of bytes to write
 * (in) append     : 0 to write from file's beginning, non-zero to write data from file's end.
 *
 * return          : 0 when successfully write all data into file, non-zeor for failures
 */
static int write_file(const char *pathname, void *data, size_t len, int append) {
    int fd = -1;
    ssize_t tmp = 0;
    size_t total = 0;
    int open_flag = O_WRONLY|O_CREAT;

    if (!data || !pathname)
        return 1;

    if (append)
        open_flag |= O_APPEND;
    else
        open_flag |=O_TRUNC;

    if ((fd = open(pathname, open_flag, 0644)) < 0) {
        perror("rmc: failed to open file to read");
        return 1;
    }

    while (total < len) {
        if ((tmp = write(fd, (BYTE *)data + total, len - total)) < 0) {
            perror("rmc: failed to write file");
            close(fd);
            return 1;
        }

        total += (size_t)tmp;
    }

    close(fd);

    return 0;
}

static void usage () {
    fprintf(stdout, USAGE);
}

/*
 * Read smbios entry table address from sysfs
 * return 0 when success
 */
static int get_smbios_entry_table_addr(uint64_t* addr){

    char *entry_buf = NULL;
    char *tmp;
    FILE *f;

    if ((f = fopen(EFI_SYSTAB_PATH, "r")) == NULL) {
        perror("rmc: Cannot get systab");
        return 1;
    }

    entry_buf = malloc(SYSTAB_LEN);

    if (!entry_buf) {
        perror("cannot allocate entry buffer");
        goto malloc_err;
    }

    while (fgets(entry_buf, SYSTAB_LEN, f) != NULL) {

        if (strncmp(entry_buf, "SMBIOS", 6))
            continue;

        /* found SMBIOS entry table */
        if ((tmp = strstr(&entry_buf[6], "=")) == NULL)
            continue;

        errno = 0;
        *addr = strtoull(++tmp, NULL,16);
        if (errno) {
            perror("strtoll() falled to convert address");
            *addr = 0;
            continue;
        } else
            break;
    }

    free(entry_buf);

malloc_err:
    fclose(f);

    return 0;

}

/* Copyright (C) 2016 Jianxun Zhang <jianxun.zhang@intel.com>
 *
 * get board's RMC fingerprint by parsing SMBIOS table (user space)
 * (out) fingerprint data to be filled. Caller needs to allocate memory
 *
 * return: 0 for success, non-zero for failures.
 */

static int get_board_fingerprint(rmc_fingerprint_t *fp) {

    int fd = -1;
    uint64_t entry_addr = 0;
    uint8_t *smbios_entry_map = NULL;
    long pg_size = 0;
    long pg_num = 0;
    uint8_t *smbios_entry_start = NULL;
    size_t entry_map_len = 0;
    size_t struct_map_len = 0;
    WORD smbios_struct_len = 0;
    uint64_t smbios_struct_addr = 0;
    uint8_t *smbios_struct_map = NULL;
    uint8_t *smbios_struct_start = NULL;
    int ret = 1;

    /* get SMBIOS entry address */

    if (get_smbios_entry_table_addr(&entry_addr)) {
        fprintf(stderr, "Cannot get valid entry tab address\n");
        return 1;
    }

    if ((fd = open("/dev/mem", O_RDONLY)) < 0) {
        perror("cannot open /dev/mem");
        return 1;
    }

    pg_size = sysconf(_SC_PAGESIZE);
    pg_num = entry_addr / pg_size;
    entry_map_len = entry_addr % pg_size + SMBIOS_ENTRY_TAB_LEN;

    smbios_entry_map = mmap(NULL, entry_map_len, PROT_READ, MAP_SHARED, fd,
            pg_num * pg_size);

    if (smbios_entry_map == MAP_FAILED) {
        perror("mmap for entry table on /dev/mem failed");
        goto err;
    }

    smbios_entry_start = smbios_entry_map + entry_addr % pg_size;

    /* parse entry point struct, call rsmp */
    ret = rsmp_get_smbios_strcut(smbios_entry_start, &smbios_struct_addr,
            &smbios_struct_len);

    if (munmap(smbios_entry_map, entry_map_len) < 0)
        perror("munmap entry failed, ignore");

    if (ret) {
        fprintf(stderr, "Cannot parse smbios entry tab\n");
        goto err;
    }

    /* mmap physical memory region of smbios struct table */
    pg_num = smbios_struct_addr / pg_size;
    struct_map_len = smbios_struct_addr % pg_size + smbios_struct_len;

    smbios_struct_map = mmap(NULL, struct_map_len, PROT_READ, MAP_SHARED, fd,
            pg_num * pg_size);

    if (smbios_struct_map == MAP_FAILED) {
        perror("mmap for struct table on /dev/mem failed");
        goto err;
    }

    smbios_struct_start = smbios_struct_map + smbios_struct_addr % pg_size;

    /* get fingerprint, call rsmp */
    ret = rsmp_get_fingerprint_from_smbios_struct(smbios_struct_start, fp);

    if (ret)
        fprintf(stderr, "Cannot get board's fingerprint\n");

    /* ! DO NOT munmap() structure's mapping, caller will access to string data in mapped region. ! */

err:
    close(fd);

    return ret;
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
    size_t len = 0;
    size_t idx = 0;
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
 * Read a policy file (cmdline or a blob) into policy file structure
 * (in) pathname        : path and name of policy file
 * (in) type            : policy type, commandline or a file blob
 *
 * return               : a pointer to policy file structure. Caller shall
 *                        free memory for returned data AND cmdline or blob
 *                        Null is returned for failures.
 */
static rmc_policy_file_t *read_policy_file(char *pathname, int type) {
    rmc_policy_file_t *tmp = NULL;
    size_t policy_len = 0;
    int ret;
    char *path_token;

    if ((tmp = calloc(1, sizeof(rmc_policy_file_t))) == NULL) {
        fprintf(stderr, "Failed to allocate memory for cmdline fragment\n\n");
        return NULL;
    }

    tmp->type = type;
    tmp->next = NULL;

    if (tmp->type == RMC_POLICY_CMDLINE) {
        ret = read_file(pathname, &tmp->cmdline_name, &policy_len);
        if (ret) {
            fprintf(stderr, "Failed to read kernel command line file %s\n\n", pathname);
            free(tmp);
            return NULL;
        }

        if (!policy_len) {
            fprintf(stderr, "Empty kernel command line file %s\n\n", pathname);
            free(tmp);
            return NULL;
        }

        tmp->blob_len = 0;
        tmp->blob = NULL;
    } else if (type == RMC_POLICY_BLOB) {
        ret = read_file(pathname, (char **)&tmp->blob, &policy_len);
        if (ret) {
            fprintf(stderr, "Failed to read policy file %s\n\n", pathname);
            free(tmp);
            return NULL;
        }
        tmp->blob_len = policy_len;
        path_token = strrchr(pathname, '/');
        if (!path_token)
            tmp->cmdline_name = strdup(pathname);
        else
            tmp->cmdline_name = strdup(path_token + 1);

        if (!tmp->cmdline_name) {
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
 * Read a record file (cmdline or a blob) into record file structure
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
    uint16_t options = 0;
    char *output_path = NULL;
    /* -C and -d could be present in a single command, with different database files specified.  */
    char *input_db_path_cap_c = NULL;
    char *input_db_path_d = NULL;
    char **input_file_blobs = NULL;
    char **input_record_files = NULL;
    char *input_fingerprint = NULL;
    char *input_cmdline_path = NULL;
    char *input_blob_name = NULL;
    rmc_fingerprint_t fingerprint;
    rmc_policy_file_t *policy_files = NULL;
    rmc_record_file_t *record_files = NULL;
    void *raw_fp = NULL;
    BYTE *db = NULL;
    BYTE *db_c = NULL;
    BYTE *db_d = NULL;
    int ret = 1;
    int i;
    int arg_num = 0;

    if (argc < 2) {
        usage();
        return 0;
    }

    /* parse options */
    opterr = 0;

    while ((c = getopt(argc, argv, "FRD:C:B:b:c:f:o:d:")) != -1)
        switch (c) {
        case 'F':
            options |= RMC_OPT_CAP_F;
            break;
        case 'R':
            options |= RMC_OPT_CAP_R;
            break;
        case 'D':
            /* we don't know nubmer of arguments for this option at this point,
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
        case 'C':
            input_db_path_cap_c = optarg;
            options |= RMC_OPT_CAP_C;
            break;
        case 'B':
            input_blob_name = optarg;
            options |= RMC_OPT_CAP_B;
            break;
        case 'o':
            output_path = optarg;
            options |= RMC_OPT_O;
            break;
        case 'c':
            input_cmdline_path = optarg;
            options |= RMC_OPT_C;
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
            if (optopt == 'F' || optopt == 'R' || optopt == 'D' || optopt == 'C' || optopt == 'B' || \
                    optopt == 'b' || optopt == 'f' || optopt == 'c' || optopt == 'f' || optopt == 'o' || optopt == 'd')
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
        uint16_t opt_o = options & (RMC_OPT_CAP_D | RMC_OPT_CAP_R | RMC_OPT_CAP_F | RMC_OPT_CAP_B);
        if (!(opt_o)) {
            fprintf(stderr, "\nWRONG: Option -o cannot be applied without -B, -D, -R or -F\n\n");
            usage();
            return 1;
        } else if (opt_o != RMC_OPT_CAP_D && opt_o != RMC_OPT_CAP_R && opt_o != RMC_OPT_CAP_F && opt_o != RMC_OPT_CAP_B) {
            fprintf(stderr, "\nWRONG: Option -o can be applied with only one of -B, -D, -R and -F\n\n");
            usage();
            return 1;
        }
    }

    /* sanity check for -R */
    if ((options & RMC_OPT_CAP_R) && (!(options & (RMC_OPT_C|RMC_OPT_B)))) {
        fprintf(stderr, "\nWRONG: At least one of -c or -b is required when -R is present\n\n");
        usage();
        return 1;
    }

    /* sanity check for -B */
    if ((options & RMC_OPT_CAP_B) && (!(options & RMC_OPT_D) || !(options & RMC_OPT_O))) {
        fprintf(stderr, "\nWRONG: -B requires -d and -o\n\n");
        usage();
        return 1;
    }

    /* get cmdline */
    if (options & RMC_OPT_CAP_C) {

        size_t db_len = 0;
        rmc_fingerprint_t fp;
        rmc_policy_file_t cmd_policy;

        /* read rmc database file */
        if (read_file(input_db_path_cap_c, (char **)&db_c, &db_len)) {
            fprintf(stderr, "Failed to read database file for command line\n\n");
            goto main_free;
        }

        /* get board fingerprint */
        if (get_board_fingerprint(&fp)) {
            fprintf(stderr, "-C Failed to generate fingerprint for this board\n\n");
            goto main_free;
        }

        /* query cmdline in database, no error message if no command line for board found */
        if (query_policy_from_db(&fp, db_c, RMC_POLICY_CMDLINE, NULL, &cmd_policy))
            goto main_free;

        fprintf(stdout, "%s", cmd_policy.cmdline_name);
    }

    /* get a file blob */
    if (options & RMC_OPT_CAP_B) {
        size_t db_len = 0;
        rmc_fingerprint_t fp;
        rmc_policy_file_t policy;

        /* read rmc database file */
        if (read_file(input_db_path_d, (char **)&db_d, &db_len)) {
            fprintf(stderr, "Failed to read database file for policy\n\n");
            goto main_free;
        }

        /* get board fingerprint */
        if (get_board_fingerprint(&fp)) {
            fprintf(stderr, "-B Failed to generate fingerprint for this board\n\n");
            goto main_free;
        }

        /* query policy in database, no error message if no policy for board found */
        if (query_policy_from_db(&fp, db_d, RMC_POLICY_BLOB, input_blob_name, &policy))
            goto main_free;

        if (!output_path) {
            fprintf(stderr, "-B internal error, with -o but no output pathname specified\n\n");
            goto main_free;
        }

        if (write_file(output_path, policy.blob, policy.blob_len, 0)) {
            fprintf(stderr, "-B failed to write policy %s to %s\n\n", input_blob_name, output_path);
            goto main_free;
        }
    }

    /* generate RMC database file */
    if (options & RMC_OPT_CAP_D) {
        int record_idx = 0;
        rmc_record_file_t *record = NULL;
        rmc_record_file_t *current_record = NULL;
        size_t db_len = 0;

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

    /* generate RMC record file, we allow both or either of a list of config files and kenrel command line.
     * That means user can choose to have a single record contains both of file blob(s) and command line for
     * the board.
     */
    if (options & RMC_OPT_CAP_R) {
        rmc_fingerprint_t fp;
        rmc_policy_file_t *policy = NULL;
        rmc_policy_file_t *current_policy = NULL;
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
            if (get_board_fingerprint(&fp)) {
                fprintf(stderr, "Failed to generate fingerprint for this board\n\n");
                goto main_free;
            }
        }

        /* read command line file and policy file into a list */
        while (input_file_blobs && input_file_blobs[policy_idx]) {
            char *s = input_file_blobs[policy_idx];

            if ((policy = read_policy_file(s, RMC_POLICY_BLOB)) == NULL) {
                fprintf(stderr, "Failed to read policy file %s\n\n", s);
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

        if (input_cmdline_path) {
            if ((policy = read_policy_file(input_cmdline_path, RMC_POLICY_CMDLINE)) == NULL) {
                fprintf(stderr, "Failed to read command line file  %s\n\n", input_cmdline_path);
                goto main_free;
            }

            /* put command line at the head of list */
            policy->next = policy_files;
            policy_files = policy;
        }

        /* call rmcl to generate record blob */
        if (rmcl_generate_record(&fp, policy_files, &record)) {
            fprintf(stderr, "Failed to generate record for this board\n\n");
            goto main_free;
        }

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

        if (get_board_fingerprint(&fingerprint)) {
            fprintf(stderr, "Cannot get board fingerprint\n");
            goto main_free;
        }

        printf("Got Fingerprint for board:\n\n");
        dump_fingerprint(&fingerprint);

        if (write_fingerprint_file(output_path, &fingerprint)) {
            fprintf(stderr, "Cannot write board fingerprint to %s\n", output_path);
            goto main_free;
        }
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
        rmc_policy_file_t *t = policy_files;
        policy_files = policy_files->next;
        free(t->blob);
        free(t->cmdline_name);
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
