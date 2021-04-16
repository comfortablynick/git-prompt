#include "util.h"

char *str_dup(const char *str)
{
    size_t len = strlen(str) + 1;
    char *p = malloc(len);
    return p ? memcpy(p, str, len) : NULL;
}

/// Allocate new memory and copy string, like `strndup()`.
///
/// Operates like `strdup()` if `n == 0`
char *str_ndup(const char *str, size_t n)
{
    // Use strnlen() if n > 0 to copy n chars
    size_t len = n == 0 ? strlen(str) : strnlen(str, n);
    char *p = malloc(len + 1);
    if (!p) return NULL;
    p[len] = '\0';
    return memcpy(p, str, len);
}

/* Split src into tokens with sentinel NULL after last token.
 * return allocated pointer-to-pointer with sentinel NULL on success,
 * or NULL on failure to allocate initial block of pointers. The number
 * of allocated pointers are doubled each time reallocation required.
 *
 * Based on: https://stackoverflow.com/a/60409814
 */
char **str_split(const char *src, const char *delim)
{
    int i = 0;     // index
    int in = 0;    // in/out flag
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
    return dest;
}
