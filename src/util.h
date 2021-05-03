#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>

#ifndef UTIL_H
#define UTIL_H

typedef struct // options_t
{
    int debug;
    // e.g. "[%b%u%m]"
    char *format;
    // show current branch
    bool show_branch;
    // show current commit sha
    bool show_commit;
    // show patch name
    bool show_patch;
    // show unknown files
    bool show_untracked;
    // show local changes
    bool show_modified;
    // timeout in milliseconds
    unsigned int timeout;
    // directory to check if not cwd
    char *directory;
} options_t;

/// Set static options struct
void set_options(options_t *options);

/// Check if static debug mode is enabled
int debug_mode();

/// Dynamically allocated buffer for reading files
typedef struct // dynbuf
{
    size_t size; // bytes allocated
    size_t len;  // bytes filled
    char *buf;
    int eof;
} dynbuf;

/// Subprocess output capture
typedef struct // capture_t
{
    dynbuf childout;
    dynbuf childerr;
    int status; // exit status that child passed (if any)
    int signal; // signal that killed the child (if any)
} capture_t;

/// Allocate new dynbuf
static void init_dynbuf(dynbuf *dbuf, int bufsize);

/// Read file into dynbuf
static ssize_t read_dynbuf(int fd, dynbuf *dbuf);

/// Free dynbuf memory as needed
void free_capture(capture_t *result);

/// Allocate new Subprocess capture
capture_t *new_capture();

/// Spawn subprocess to capture command
capture_t *capture_child(char *const argv[]);

/// Alternative to strtol which allows '+' and '-' prefixes
int strtoint_n(const char *str, int n);

/// Allocate new memory and copy string
char *str_dup(const char *str);

/// Allocate new memory and copy string, like `strndup()`
///
/// Operates like `strdup()` if `n == 0`
char *str_ndup(const char *str, size_t n);

/// Split `src` into tokens with sentinel `NULL` after last token.
///
/// Return allocated pointer-to-pointer with sentinel `NULL` on success,
/// or `NULL` on failure to allocate initial block of pointers. The number
/// of allocated pointers are doubled each time reallocation required.
///
/// Set `n` to the size of pointer-to-pointer array if `n != NULL`
/// Based on: https://stackoverflow.com/a/60409814
char **str_split(const char *src, const char *delim, size_t *n);

#endif // #define UTIL_H
