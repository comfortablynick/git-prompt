#include <stdbool.h>   // for bool
#include <stdio.h>     // for size_t
#include <sys/types.h> // for ssize_t

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

