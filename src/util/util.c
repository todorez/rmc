#include <rmc_util.h>

void *memset(void *s, rmc_uint8_t c, rmc_size_t n) {
    rmc_uint8_t *p = (rmc_uint8_t *)s;
    while (n--)
        *p++ = c;
    return s;
}

int strncmp(const char *s1, const char *s2, rmc_size_t n) {
    while (n--) {
        if (*s1 == '\0' || *s1 != *s2)
            return *s1 - *s2;
        s1++;
        s2++;
    }

    return 0;
}

void *memcpy(void *d, const void *s, rmc_size_t n) {
    rmc_uint8_t *p = d;
    rmc_uint8_t *q = (rmc_uint8_t *)s;

    while (n--)
        *p++ = *q++;

    return d;
}

rmc_size_t strlen(const char *s) {
    rmc_size_t ret = 0;

    while (*s++ != '\0')
      ret++;
    return ret;
}

char *strncpy(char *d, const char *s, rmc_size_t n) {
    rmc_size_t i;

    for (i = 0; i < n && s[i] != '\0'; i++)
        d[i] = s[i];

    for (; i < n; i++)
        d[i] = '\0';

    return d;
}
