/* Copyright (C) 2016 Jianxun Zhang <jianxun.zhang@intel.com>
 *
 * RMC API implementation for Linux user space
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <dirent.h>
#include <ctype.h>

#include <rmcl.h>
#include <rsmp.h>

#define EFI_SYSTAB_PATH  "/sys/firmware/efi/systab"
#define SYSTAB_LEN       4096             /* assume 4kb is enough...*/
#define DB_DUMP_DIR      "./rmc_db_dump"  /* directory to store db data dump */

int read_file(const char *pathname, char **data, rmc_size_t* len) {
    int fd = -1;
    struct stat s;
    off_t total = 0;
    void *buf = NULL;
    rmc_size_t byte = 0;
    rmc_ssize_t tmp = 0;

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
            perror("rmc: failed to read file");
            free(buf);
            close(fd);
            return 1;
        }

        byte += (rmc_size_t)tmp;
    }

    *data = buf;
    *len = byte;

    close(fd);
    return 0;
}

int write_file(const char *pathname, void *data, rmc_size_t len, int append) {
    int fd = -1;
    rmc_ssize_t tmp = 0;
    rmc_size_t total = 0;
    int open_flag = O_WRONLY|O_CREAT;

    if (!data || !pathname)
        return 1;

    if (append)
        open_flag |= O_APPEND;
    else
        open_flag |= O_TRUNC;

    if ((fd = open(pathname, open_flag, 0644)) < 0) {
        perror("rmc: failed to open file to read");
        return 1;
    }

    while (total < len) {
        if ((tmp = write(fd, (rmc_uint8_t *)data + total, len - total)) < 0) {
            perror("rmc: failed to write file");
            close(fd);
            return 1;
        }

        total += (rmc_size_t)tmp;
    }

    close(fd);

    return 0;
}

/*
 * Read smbios entry table address from sysfs
 * return 0 when success
 */
static int get_smbios_entry_table_addr(rmc_uint64_t* addr){

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

void rmc_free_fingerprint(rmc_fingerprint_t *fp) {
    int fp_idx;

    if(!fp)
        return;

    for (fp_idx=0; fp_idx < RMC_FINGER_NUM; fp_idx++)
        free(fp->rmc_fingers[fp_idx].value);
}

void rmc_free_file(rmc_file_t *file) {
    free(file->blob);
}

int rmc_get_fingerprint(rmc_fingerprint_t *fp) {

    int fd = -1;
    rmc_uint64_t entry_addr = 0;
    rmc_uint8_t *smbios_entry_map = NULL;
    long pg_size = 0;
    long pg_num = 0;
    rmc_uint8_t *smbios_entry_start = NULL;
    rmc_size_t entry_map_len = 0;
    rmc_size_t struct_map_len = 0;
    rmc_uint16_t smbios_struct_len = 0;
    rmc_uint64_t smbios_struct_addr = 0;
    rmc_uint8_t *smbios_struct_map = NULL;
    rmc_uint8_t *smbios_struct_start = NULL;
    int ret = 1;
    int i;
    int j;

    if (!fp)
        return 1;

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
        perror("munmap smbios entry failed, ignore");

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

    if (ret) {
        fprintf(stderr, "Cannot get board's fingerprint\n");
        goto err_unmap;
    }

    /* what rsmp returned for a finger's value is on stack. We duplicate them here
     * before unmap the memory and caller can free them with rmc_free_fingerprint() later
     * The other fields are hardcoded in initialize_fingerprint(), not in mapped region,
     * so we don't copy them.
     */
    for (i = 0; i < RMC_FINGER_NUM; i++) {
        fp->rmc_fingers[i].value = strdup(fp->rmc_fingers[i].value);
        if (!fp->rmc_fingers[i].value) {
            perror("insufficient memory for obtained fingerprint");
            for (j = 0; j < i; j++)
                free(fp->rmc_fingers[j].value);
            ret = 1;
            break;
        }
    }

err_unmap:
    if (munmap(smbios_struct_map, struct_map_len) < 0)
        perror("munmap smbios struct failed, ignore");

err:
    close(fd);

    return ret;
}

int rmc_query_file_by_fp(rmc_fingerprint_t *fp, char *db_pathname, char *file_name, rmc_file_t *file) {
    rmc_uint8_t *db = NULL;
    rmc_size_t db_len = 0;
    int ret = 1;
    rmc_uint8_t *blob = NULL;

    /* ToDo: We should use file seeking when traversing a database
     * file instead of load whole DB into mem.
     *
     * read rmc database file
     */
    if (read_file(db_pathname, (char **)&db, &db_len)) {
        fprintf(stderr, "Failed to read database file\n\n");
        return ret;
    }

    /* query policy in database */
    if(query_policy_from_db(fp, db, RMC_GENERIC_FILE, file_name, file))
        goto free_db;

    /* the returned file blob is actually in db memory region,
     * we need to copy the data to a buffer which will be returned
     *  to the caller, so that we can free the db memory safely here
     */
    blob = malloc(file->blob_len);

    if (blob) {
        memcpy(blob, file->blob, file->blob_len);
        file->blob = blob;
        ret = 0;
    } else
        perror("insufficient memory for the queried file");

free_db:
    free(db);

    return ret;
}

int rmc_gimme_file(char* db_pathname, char *file_name, rmc_file_t *file) {
    rmc_fingerprint_t fp;
    int ret = 1;

    /* get board fingerprint */
    if (rmc_get_fingerprint(&fp)) {
        fprintf(stderr, "-B Failed to generate fingerprint for this board\n\n");
        return ret;
    }

    ret = rmc_query_file_by_fp(&fp, db_pathname, file_name, file);

    rmc_free_fingerprint(&fp);

    return ret;
}

static char *str2hex(const char *in) {
    int i , len = strlen(in);
    char *out = calloc(2*len+1, sizeof(char));
    for (i = 0; i < len; i++) {
        sprintf(&out[2*i], "%x", in[i]);
    }
    out[2*len+1] = '\0';
    return out;
}

static int mkpath(const char *dir) {
    char tmp[PATH_MAX], *path = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp),"%s",dir);
    len = strlen(tmp);

    /* mark end of path */
    if(tmp[len - 1] == '/')
        tmp[len - 1] = 0;

    for(path = tmp + 1; *path; path++) {
        if(*path == '/') {
            /* mark end of directory chunk */
            *path = 0;
            /* try to create this portion of the path. Failing here is valid
             * if the directory already exists. */
            mkdir(tmp, S_IRWXU);
            /* restore directory separator */
            *path = '/';
        }
    }
    return mkdir(tmp, S_IRWXU);
}

int dump_db(char *db_pathname, char *output_path) {
    rmc_meta_header_t meta_header;
    rmc_db_header_t *db_header = NULL;
    rmc_record_header_t record_header;
    rmc_uint64_t record_idx = 0;   /* offset of each reacord in db*/
    rmc_uint64_t meta_idx = 0;     /* offset of each meta in a record */
    rmc_uint64_t file_idx = 0;     /* offset of file in a meta */
    rmc_file_t file;
    char *out_dir = NULL, *out_name = NULL, *tmp_dir_name = NULL;
    rmc_size_t db_len = 0;
    rmc_uint8_t *rmc_db = NULL;
    DIR *tmp_dir = NULL;

    if (read_file(db_pathname, (char **)&rmc_db, &db_len)) {
        fprintf(stderr, "Failed to read database file\n\n");
        return 1;
    }

    db_header = (rmc_db_header_t *)rmc_db;

    /* sanity check of db */
    if (is_rmcdb(rmc_db))
        return 1;

    /* query the meta. idx: start of record */
    record_idx = sizeof(rmc_db_header_t);
    while (record_idx < db_header->length) {
        memcpy(&record_header, rmc_db + record_idx,
            sizeof(rmc_record_header_t));

        /* directory name is fingerprint signature with stripped special chars*/
        tmp_dir_name = str2hex((const char *)record_header.signature.raw);

        if(output_path)
            asprintf(&out_dir, "%s/%s/", output_path, tmp_dir_name);
        else
            asprintf(&out_dir, "%s/%s/", DB_DUMP_DIR, tmp_dir_name);
        if ((tmp_dir = opendir(out_dir))) {
            /* Directory exists */
            closedir(tmp_dir);
            free(tmp_dir_name);
        } else if (ENOENT == errno) {
            /* Directory does not exist, try to create it. */
            if(mkpath(out_dir)) {
                fprintf(stderr, "Failed to create %s directory\n\n", out_dir);
                free(out_dir);
                free(tmp_dir_name);
                return 1;
            }
        } else {
            /* Some other error occured */
            free(out_dir);
            free(tmp_dir_name);
            return 1;
        }

        /* find meta */
        for (meta_idx = record_idx + sizeof(rmc_record_header_t);
            meta_idx < record_idx + record_header.length;) {
            memcpy(&meta_header, rmc_db + meta_idx, sizeof(rmc_meta_header_t));
            file_idx = meta_idx + sizeof(rmc_meta_header_t);
            rmc_ssize_t name_len = strlen((char *)&rmc_db[file_idx]) + 1;
            file.blob = &rmc_db[file_idx + name_len];
            file.blob_len = meta_header.length - sizeof(rmc_meta_header_t) -
                name_len;
            file.next = NULL;
            file.type = RMC_GENERIC_FILE;
            asprintf(&out_name, "%s%s", out_dir, (char *)&rmc_db[file_idx]);
            /* write file to dump directory */
            if (write_file((const char *)out_name, file.blob, file.blob_len, 0))
                return 1;

            /* next meta */
            meta_idx += meta_header.length;
            free(out_name);
        }
        /* next record */
        record_idx += record_header.length;
        free(out_dir);
    }
    return 0;
}
