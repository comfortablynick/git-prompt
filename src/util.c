#include "util.h"
#include <ctype.h>  // for isspace
#include <stdio.h>  // for perror, NULL, size_t
#include <stdlib.h> // for malloc, realloc
#include <string.h> // for memcpy, strlen, strchr, strnlen

int strtoint_n(const char *str, int n)
{
    int nZ[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int sign = 1;
    int place = 1;
    int ret = 0;

    for (int i = n - 1; i >= 0; --i, place *= 10) {
        int c = str[i];
        switch (c) {
        case 45: // '-'
            if (i == 0)
                sign = -1;
            else
                return -1;
            break;
        case 43: // '+'
            if (i == 0)
                sign = 1;
            else
                return -1;
            break;
        default:
            if (c >= 48 && c <= 57)
                ret += nZ[c - 48] * place;
            else
                return -1;
        }
    }
    return sign * ret;
}

char *str_dup(const char *str)
{
    const size_t len = strlen(str) + 1;
    char *p = malloc(len);
    return p ? memcpy(p, str, len) : NULL;
}

char *str_ndup(const char *str, const size_t n)
{
    // Use strnlen() if n > 0 to copy n chars
    const size_t len = n == 0 ? strlen(str) : strnlen(str, n);
    char *p = malloc(len + 1);
    if (!p) return NULL;
    p[len] = '\0';
    return memcpy(p, str, len);
}

char **str_split(const char *src, const char *delim, size_t *n)
{
    int i = 0;      // index
    int in = 0;     // in/out flag
    int nptrs = 10; // initial # of ptrs to allocate (must be > 0)
    char **dest = NULL;
    const char *p = src;
    const char *ep = p;

    // allocate ptrs
    if (!(dest = malloc(nptrs * sizeof *dest))) {
        perror("malloc-dest");
        return NULL;
    }
    *dest = NULL;

    for (;;) {
        if (!*ep || strchr(delim, *ep)) {
            size_t len = ep - p;
            if (in && len) {
                if (i == nptrs - 1) {
                    // realloc dest to temporary pointer/validate
                    void *tmp = realloc(dest, 2 * nptrs * sizeof *dest);
                    if (!tmp) {
                        perror("realloc-dest");
                        break; // don't exit, original dest still valid
                    }
                    dest = tmp; // assign reallocated block to dest
                    nptrs *= 2; // increment allocated pointer count
                }
                // allocate/validate storage for token
                if (!(dest[i] = malloc(len + 1))) {
                    perror("malloc-dest[i]");
                    break;
                }
                memcpy(dest[i], p, len); // copy len chars to storage
                dest[i++][len] = 0;      // nul-terminate, advance index
                dest[i] = NULL;          // set next pointer NULL
            }
            if (!*ep) // if at end, break
                break;
            in = 0;
        } else {             // normal word char
            if (!in) p = ep; // update start to end-pointer
            in = 1;
        }
        ep++;
    }
    if (n) *n = i;
    return dest;
}

size_t str_squish(char *str, bool trim)
{
    char *save = str;
    char *from = str;
    if (trim)
        while (isspace(*from)) ++from;
    for (; *from; ++from) {
        if (isspace(*from) && isspace(*(from - 1))) continue;
        *str++ = *from;
    }
    if (trim && isspace(*(str - 1))) --str;
    *str = '\0';
    return (str - save);
}
