#include "util.h"

char *str_dup(const char *str)
{
    const size_t len = strlen(str) + 1;
    char *p = malloc(len);
    return p ? memcpy(p, str, len) : NULL;
}

/// Allocate new memory and copy string, like `strndup()`.
///
/// Operates like `strdup()` if `n == 0`
char *str_ndup(const char *str, const size_t n)
{
    // Use strnlen() if n > 0 to copy n chars
    const size_t len = n == 0 ? strlen(str) : strnlen(str, n);
    char *p = malloc(len + 1);
    if (!p) return NULL;
    p[len] = '\0';
    return memcpy(p, str, len);
}


/// Split s into tokens
char **string_split(const char *s, char sep, size_t *n)
{
    const unsigned int rate = 8;
    char **out = calloc(rate, sizeof(char *));
    if (!out) return NULL;
    size_t ctr = 0;
    unsigned int blocks = rate;
    const char *where;
    while ((where = strchr(s, sep))) {
        size_t size = (size_t)(where - s);
        out[ctr] = str_ndup(s, size);
        if (!out[ctr]) return NULL;
        ctr++;
        if (ctr == blocks) {
            blocks += rate;
            out = (char **)realloc(out, sizeof(char *) * blocks);
            if (!out) return NULL;
        }
        s += size + 1;
    }
    if (s[0]) {
        out[ctr] = str_ndup(s, 0);
        if (!out[ctr]) return NULL;
        ctr++;
    }
    out = realloc(out, sizeof(char *) * (ctr + 1));
    if (!out) return NULL;
    out[ctr] = NULL;

    if (n) *n = ctr;

    return out;
}

/* Split `src` into tokens with sentinel `NULL` after last token.
 * 
 * Return allocated pointer-to-pointer with sentinel `NULL` on success,
 * or `NULL` on failure to allocate initial block of pointers. The number
 * of allocated pointers are doubled each time reallocation required.
 *
 * Set `n` to the size of pointer-to-pointer array if `n != NULL`
 * Based on: https://stackoverflow.com/a/60409814
 */
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
