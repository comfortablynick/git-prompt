#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#pragma once

struct options
{
    // debug verbosity
    int debug;
    // debug logger level
    int log_level;
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
    uint8_t timeout;
    // directory to check if not cwd
    char *directory;
};

/// Allocate new options struct
struct options *new_options();

/// Set static options struct
void set_options(struct options *options);

/// Free allocated memory on options object as needed
void free_options(struct options *options);

/// Check if static debug mode is enabled
int debug_mode();

/// Dynamically allocated buffer for reading files
struct dynbuf
{
    size_t size; // bytes allocated
    size_t len;  // bytes filled
    char *buf;
    int eof;
};

/// Subprocess output capture
struct capture
{
    struct dynbuf childout;
    struct dynbuf childerr;
    int status; // exit status that child passed (if any)
    int signal; // signal that killed the child (if any)
};

/// Allocate new dynbuf
static void init_dynbuf(struct dynbuf *dbuf, int bufsize);

/// Read file into dynbuf
static ssize_t read_dynbuf(int fd, struct dynbuf *dbuf);

/// Free dynbuf memory as needed
void free_capture(struct capture *result);

/// Allocate new Subprocess capture
struct capture *new_capture();

/// Spawn subprocess to capture command
struct capture *capture_child(char *const argv[]);

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
