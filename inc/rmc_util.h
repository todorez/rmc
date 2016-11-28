/* util.c - implementation of basic C functions
 * In EFI land, we don't have all of these functions as what we do
 * from std C lib.
 */

#ifndef INC_RMC_UTIL_H_
#define INC_RMC_UTIL_H_
#include <rmc_types.h>

void *memset(void *s, rmc_uint8_t c, rmc_size_t n);

char *strncpy(char *dest, const char *src, rmc_size_t n);

char *strcpy(char *dest, const char *src);

rmc_size_t strlen(const char *s);

void *memcpy(void *dest, const void *src, rmc_size_t n);

int strncmp(const char *s1, const char *s2, rmc_size_t n);

#endif
