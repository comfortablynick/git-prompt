#include <stdio.h>
#include <stdint.h>

#pragma once

struct git_repo
{
    char *branch;
    char *commit;
    uint8_t changed;
    uint8_t untracked;
    uint8_t unmerged;
    uint8_t ahead;
    uint8_t behind;
    // Helper functions
    void (*sprint)(const struct git_repo *, char *);
    void (*free)(struct git_repo *);
    int (*set_branch)(struct git_repo *, const char *, size_t);
    int (*set_commit)(struct git_repo *, const char *, size_t);
    int (*set_ahead_behind)(struct git_repo *, char *);
};

/// Allocate new git_repo struct
struct git_repo *new_git_repo();
