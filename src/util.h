#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

#pragma once

/// Store options set from command line
struct options
{
    /// Debug verbosity
    int debug;
    /// Output format (print-f style) string e.g. "[%b%u%m]"
    char *format;
    /// Show current branch
    bool show_branch;
    /// Show current commit sha
    bool show_commit;
    /// Show patch name
    bool show_patch;
    /// Show untracked (unknown) files
    bool show_untracked;
    /// Show local changes
    bool show_modified;
    /// Timeout in milliseconds for command to complete
    uint8_t timeout;
    /// Directory to use for git commands
    char *directory;
    /// Set static options object
    void (*set)(const struct options *);
    /// Free options object
    void (*free)(struct options *);
    /// Print options object
    void (*sprint)(const struct options *, char *);
};

/// Allocate new options struct
struct options *new_options();

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

/// Squeeze whitespace in char array and return new size
size_t str_collapse_whitespace(char *str);
