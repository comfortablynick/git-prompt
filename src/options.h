#pragma once

#include <stdbool.h>  // for bool
#include <stdint.h>   // for uint8_t

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
