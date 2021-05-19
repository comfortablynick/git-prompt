#pragma once

#include <stdint.h>  // for uint8_t
#include <stdio.h>   // for size_t, FILE

struct options;

#ifndef GIT_HASH_LEN
#define GIT_HASH_LEN 7
#endif

/// Hold data parsed from git status
struct git_repo
{
    char *branch;
    char *commit;
    uint8_t changed;
    uint8_t untracked;
    uint8_t unmerged;
    uint8_t ahead;
    uint8_t behind;

    /// Set buf to debug representation of git_repo struct
    void (*sprint)(const struct git_repo *self, char *buf);
    /// Free git_repo struct and internal pointers if they exist
    void (*free)(struct git_repo *self);
    /// Set git_repo branch string
    int (*set_branch)(struct git_repo *self, const char *branch, size_t len);
    /// Set git_repo commit string
    int (*set_commit)(struct git_repo *self, const char *commit, size_t len);
    /// Set git_repo ahead/behind from string
    int (*set_ahead_behind)(struct git_repo *self, char *buf);
};

/// Parse git_repo according to format string
void parse_result(struct git_repo *repo, const char *format, FILE *stream);

/// Allocate new git_repo struct
struct git_repo *new_git_repo();

/// Parse output of git status to repo
void parse_porcelain(struct git_repo *repo, struct options *opts);
