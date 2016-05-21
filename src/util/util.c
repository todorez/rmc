#include <rmc_util.h>

void *memset(void *s, BYTE c, size_t n) {
    BYTE *p = (BYTE *)s;
    while (--n)
        *p++ = c;
    return s;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (--n) {
        if (*s1 != *s2 || *s1 == '\0')
            break;
        s1++;
        s2++;
    }

    return *s1 - *s2;
}

void *memcpy(void *d, const void *s, size_t n) {
    BYTE *p = d;
    BYTE *q = (BYTE *)s;

    while (--n)
        *p++ = *q++;

    return d;
}

size_t strlen(const char *s) {
    size_t ret = 0;

    while (*s++ != '\0')
      ret++;
    return ret;
}

char *strncpy(char *d, const char *s, size_t n) {
    size_t i;

    for (i = 0; i < n && s[i] != '\0'; i++)
        d[i] = s[i];

    for (; i < n; i++)
        d[i] = '\0';

    return d;
}
