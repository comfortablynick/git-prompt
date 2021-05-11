#include "log.h"
#include "util.h"
#include <bits/getopt_core.h>
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef GIT_HASH_LEN
#define GIT_HASH_LEN 7
#endif

#ifndef FMT_STRING
#define FMT_STRING "%b@%c"
#endif

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
    void (*sprint)(const struct git_repo*, char*);
    void (*free)(struct git_repo*);
    int (*set_branch)(struct git_repo*, const char*, size_t);
    int (*set_commit)(struct git_repo*, const char*, size_t);
    int (*set_ahead_behind)(struct git_repo*, char*);
};

/// Completely free git_repo struct
static void _git_repo_free(struct git_repo *self)
{
    if (!self) return;
    if (self->branch) free(self->branch);
    if (self->commit) free(self->commit);
    free(self);
}

/// Set buf to debug repr of git_repo
static void _git_repo_debug(const struct git_repo *self, char *buf)
{
    sprintf(buf,
            "Repo results:\n"
            "Commit:    %s\n"
            "Branch:    %s\n"
            "Changed:   %d\n"
            "Untracked: %d\n"
            "Ahead:     %d\n"
            "Behind:    %d\n",
            self->commit, self->branch, self->changed, self->untracked, self->ahead, self->behind);
}

/// Set branch name in git_repo struct
static int _git_repo_set_branch(struct git_repo *repo, const char *branch, size_t len)
{
    if (repo->branch) free(repo->branch);
    repo->branch = str_ndup(branch, len);
    return !!repo->branch;
}

/// Set branch name in git_repo struct
static int _git_repo_set_commit(struct git_repo *repo, const char *commit, size_t len)
{
    if (repo->commit) free(repo->commit);
    repo->commit = str_ndup(commit, len);
    return !!repo->commit;
}

/// Set number of commits ahead/behind upstream
static int _git_repo_set_ahead_behind(struct git_repo *repo, char *buf)
{
    log_debug("Ahead/behind: %s", buf);
    int found = 0;
    while (*buf) {
        if (isdigit(*buf)) {
            int val = (int)strtol(buf, &buf, 10);
            if (found == 0) {
                repo->ahead = val;
            } else {
                repo->behind = val;
            }
            ++found;
        } else {
            ++buf;
        }
    }
    return !!found;
}

/// Allocate new git_repo struct
struct git_repo *new_git_repo() {
    struct git_repo *repo = calloc(1, sizeof(struct git_repo));
    repo->sprint = _git_repo_debug;
    repo->free = _git_repo_free;
    repo->set_branch = _git_repo_set_branch;
    repo->set_commit = _git_repo_set_commit;
    repo->set_ahead_behind = _git_repo_set_ahead_behind;
    return repo;
}

void parse_porcelain(struct git_repo *repo, struct options *opts)
{
    char *args[] = {
        "git",      "-C", opts->directory, "status", "--porcelain=2", "--untracked-files=normal",
        "--branch", NULL};
    if (!opts->show_untracked) args[5] = "--untracked-files=no";
    struct capture *output;
    if ((output = capture_child(args))) {
        const char *cstdout = output->childout.buf;
        char **split;
        int line = 1;
        size_t line_ct;
        if ((split = str_split(cstdout, "\n", &line_ct))) {
            log_debug("Stdout contains %zu lines", line_ct);
            for (char **p = split; *p; ++p) {
                log_debug("L%d: %s", line, *p);
                char *tmp;
                const char *commit = "branch.oid";
                const char *branch = "branch.head";
                const char *ab = "branch.ab";
                if ((tmp = strstr(*p, commit))) {
                    if ((!repo->set_commit(repo, tmp + strlen(commit) + 1, GIT_HASH_LEN))) {
                        fputs("Error setting repo commit", stderr);
                        break;
                    }
                } else if ((tmp = strstr(*p, branch))) {
                    if ((!repo->set_branch(repo, tmp + strlen(branch) + 1, 0))) {
                        fputs("Error setting repo branch", stderr);
                        break;
                    }
                } else if ((tmp = strstr(*p, ab))) {
                    if ((!repo->set_ahead_behind(repo, tmp + strlen(ab) + 1))) {
                        fputs("Error setting repo ahead/behind", stderr);
                        break;
                    }
                } else if (*p[0] == '?') {
                    ++repo->untracked;
                } else if (*p[0] == '1') {
                    ++repo->changed;
                }
                free(*p);
                ++line;
            }
            free(split);
        }
        char repo_debug[1024];
        repo->sprint(repo, repo_debug);
        log_debug("%s", repo_debug);
    } else {
        log_error("Error getting command output: %s", args[0]);
    }
    free_capture(output);
}

/// Parse cli arguments into options struct
struct options *parse_args(int argc, char **argv)
{
    struct options *options = new_options();
    if (!options) return NULL;
    int opt;
    while ((opt = getopt(argc, argv, "hqdf:")) != -1) {
        switch (opt) {
        case 'd':
            log_set_quiet(false);
            ++options->debug;
            break;
        case 'q':
            options->debug = -1;
            log_set_quiet(true);
            break;
        case 'f':
            options->format = str_ndup(optarg, 0);
            break;
        default:
            fprintf(stderr, "Usage: %s [-d] [-f FMT] <directory>\n", basename(argv[0]));
            free(options);
            exit(EXIT_FAILURE);
        }
    }
    if (argv[optind]) {
        options->directory = realpath(argv[optind], NULL);
    } else {
        options->directory = getcwd(NULL, 0);
    }
    return options;
}

/// Parse format string to options_t object
void parse_format(struct options *opts)
{
    for (char *fmt = opts->format; *fmt; ++fmt) {
        if (*fmt == '%') {
            ++fmt;
            switch (*fmt) {
            case 'b':
                opts->show_branch = true;
                break;
            case 'c':
                opts->show_commit = true;
                break;
            case 'u':
                opts->show_untracked = true;
                break;
            case 'm':
                opts->show_modified = true;
                break;
            default:
                log_error("error: invalid format string token: %%%c\n", *fmt);
                fprintf(stderr, "error: invalid format string token: %%%c\n", *fmt);
                exit(1);
            }
        }
    }
}

void parse_result(struct git_repo *repo, struct options *opts, FILE *stream)
{
    for (char *fmt = opts->format; *fmt; ++fmt) {
        if (*fmt == '%') {
            ++fmt;
            switch (*fmt) {
            case 'b':
                fprintf(stream, "%s", repo->branch);
                break;
            case 'c':
                fprintf(stream, "%s", repo->commit);
                break;
            case 'u':
                fprintf(stream, "?%d", repo->untracked);
                break;
            case 'm':
                fprintf(stream, "*%d", repo->changed);
                break;
            default:
                log_error("error: invalid format string token: %%%c\n", *fmt);
                fprintf(stderr, "error: invalid format string token: %%%c\n", *fmt);
                exit(1);
            }
        } else {
            fputc(*fmt, stream);
        }
    }
    fflush(stream);
}

int main(int argc, char **argv)
{
    struct options *options = parse_args(argc, argv);
    if (!options->format) options->format = str_ndup(FMT_STRING, 0);
    parse_format(options);
    set_options(options);

    int log_level = 0 - options->debug;
    log_set_level(log_level);
#ifndef LOG_USE_COLOR
    log_debug("Set -DLOG_USE_COLOR for color logging");
#endif

    if (log_level <= LOG_TRACE && argc > 1) {
        // Print command line args (after prog name)
        for (int i = 1; i < argc; ++i) log_trace("argv[%d]: %s", i, argv[i]);
    }
    log_debug("Parsed options:\n"
              "Debug:         %d\n"
              "Format:        %s\n"
              "Directory:     %s\n"
              "Show branch:   %d\n"
              "Show commit:   %d\n"
              "Show unknown:  %d\n"
              "Show modified: %d\n",
              options->debug, options->format, options->directory, options->show_branch,
              options->show_commit, options->show_untracked, options->show_modified);

    struct git_repo *repo = new_git_repo();
    parse_porcelain(repo, options);
    FILE *stream;
    char *buf;
    size_t buflen;
    stream = open_memstream(&buf, &buflen);
    parse_result(repo, options, stream);
    fclose(stream);
    log_debug("buf=%s, len=%zu", buf, buflen);
    free(buf);

    free_options(options);
    repo->free(repo);
}
