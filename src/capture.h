#pragma once

#include <stdio.h> // for size_t

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

    void (*free)(struct capture *);
};

/// Allocate new Subprocess capture
struct capture *new_capture();

/// Spawn subprocess to capture command
struct capture *capture_child(char *const argv[]);
