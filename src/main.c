#include "log.h"
#include "util.h"
#include <assert.h>
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
    void (*sprint)(const struct git_repo *, char *);
    void (*free)(struct git_repo *);
    int (*set_branch)(struct git_repo *, const char *, size_t);
    int (*set_commit)(struct git_repo *, const char *, size_t);
    int (*set_ahead_behind)(struct git_repo *, char *);
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
            "Commit:    %s\n"
            "Branch:    %s\n"
            "Changed:   %d\n"
            "Untracked: %d\n"
            "Ahead:     %d\n"
            "Behind:    %d",
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
struct git_repo *new_git_repo()
{
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
        log_debug("Repo results:\n%s", repo_debug);
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
        case 'v':
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
            fprintf(stderr, "Usage: %s [-h] [-V] [-v] [-t MSECS] [-f FORMAT] [dir]\n%s",
                    basename(argv[0]),
                    "\nFlags:\n"
                    "  -h   show this help message and exit\n"
                    "  -V   show program version\n"
                    "  -v   increase console debug verbosity (-v, -vv, -vvv)\n"
                    "\nArguments:\n"
                    "  -t   timeout threshold, in milliseconds\n"
                    "  -f   tokenized string that determines output\n"
                    "       %b  show branch\n"
                    "       %c  show commit hash\n"
                    "       %u  indicate unknown (untracked) files with '?'\n"
                    "       %U  show count of unknown files\n"
                    "       %m  indicate uncommitted changes (modified/added/removed) with '*'\n"
                    "       %M  show count of uncommitted changes\n"
                    "       %n  insert newline\n"
                    "       %%  show '%'\n"
                    "  dir  location of git repo (default is current working directory)\n"
                    "\nEnvironment:\n"
                    "  $GITPROMPT_FORMAT  variable containing FORMAT string");

            fprintf(stderr, " (default=\"%s\")\n", FMT_STRING);
            free(options);
            exit(EXIT_FAILURE);
        }
    }
    if (argv[optind]) {
        options->directory = realpath(argv[optind], NULL);
    } else {
        options->directory = getcwd(NULL, 0);
    }
    if (!options->format) {
        char *format = getenv("GITPROMPT_FORMAT");
        if (!format) format = FMT_STRING;
        options->format = str_ndup(format, 0);
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
                break;
            }
        }
    }
}

void parse_result(struct git_repo *repo, const char *format, FILE *stream)
{
    for (const char *fmt = format; *fmt; ++fmt) {
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
                if (repo->untracked) putc('?', stream);
                break;
            case 'U':
                if (repo->untracked) fprintf(stream, "%d", repo->untracked);
                break;
            case 'm':
                if (repo->changed) putc('*', stream);
                break;
            case 'M':
                if (repo->changed) fprintf(stream, "%d", repo->changed);
                break;
            case 'n':
                putc('\n', stream);
                break;
            case '\\':
                if (*++fmt == 'n') putc('\n', stream);
                break;
            default:
                log_error("error: invalid format string token: %%%c\n", *fmt);
                fprintf(stderr, "error: invalid format string token: %%%c\n", *fmt);
                exit(1);
            }
            } else if (*fmt == '\\') {
                if (*++fmt == 'n') {
                    putc('\n', stream);
                } else {
                    log_warn("invalid escape sequence in format string: \\%s", fmt);
                }
        } else {
            fputc(*fmt, stream);
        }
    }
    fflush(stream);
}

/// Set up repo struct and test result of parse function
void test_parse()
{
    struct git_repo repo = {.branch = "test",
                            .commit = "abcd1234",
                            .ahead = 1,
                            .behind = 2,
                            .changed = 10,
                            .untracked = 100};
    const char *format = "%b@%c %m %u";
    const char *expected = "test@abcd1234 *10 ?100";
    FILE *stream;
    char *buf;
    size_t buflen;
    stream = open_memstream(&buf, &buflen);
    parse_result(&repo, format, stream);
    fclose(stream);
    log_debug("Test results\n"
              "Result:    {buf=%s, len=%zu}\n"
              "Expected:  {buf=%s, len=%zu}\n"
              "Match:     %d",
              buf, buflen, expected, strlen(expected), (strcmp(buf, expected) == 0));
    assert((strcmp(buf, expected) == 0));
    free(buf);
}

int main(int argc, char **argv)
{
    struct options *options = parse_args(argc, argv);
    parse_format(options);
    options->set(options);

    int log_level = 0 - options->debug;
    log_set_level(log_level);
#ifndef LOG_USE_COLOR
    log_debug("Set -DLOG_USE_COLOR for color logging");
#endif
    if (log_level <= LOG_TRACE && argc > 1) {
        // Print command line args (after prog name)
        for (int i = 1; i < argc; ++i) log_trace("argv[%d]: %s", i, argv[i]);
    }
    if (log_level <= LOG_DEBUG) {
        char opts_debug[1024];
        options->sprint(options, opts_debug);
        log_debug("Parsed options:\n%s", opts_debug);
    }
    struct git_repo *repo = new_git_repo();
    parse_porcelain(repo, options);
    parse_result(repo, options->format, stdout);
    options->free(options);
    repo->free(repo);

    // Testing
    // putchar('\n');
    // test_parse();
}
