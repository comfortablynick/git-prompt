#include "log.h"
#include "util.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef GIT_HASH_LEN
#define GIT_HASH_LEN 7
#endif

typedef struct git_repo
{
    char *branch;
    char *commit;
    int changed;
    int untracked;
    int unmerged;
    int ahead;
    int behind;
} git_repo;

/// Allocate new git_repo struct
git_repo *new_git_repo() { return calloc(1, sizeof(git_repo)); }

/// Completely free git_repo struct
void free_git_repo(git_repo *repo)
{
    if (!repo) return;
    if (repo->branch) free(repo->branch);
    if (repo->commit) free(repo->commit);
    free(repo);
}

/// Set branch name in git_repo struct
int git_repo_set_branch(git_repo *repo, const char *branch, size_t len)
{
    if (repo->branch) free(repo->branch);
    repo->branch = str_ndup(branch, len);
    return !!repo->branch;
}

/// Set branch name in git_repo struct
int git_repo_set_commit(git_repo *repo, const char *commit, size_t len)
{
    if (repo->commit) free(repo->commit);
    repo->commit = str_ndup(commit, len);
    return !!repo->commit;
}

/// Set number of commits ahead/behind upstream
int git_repo_set_ahead_behind(git_repo *repo, char *buf)
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

void parse_porcelain()
{
    char *args[] = {"git", "status", "--porcelain=2", "--untracked-files=normal", "--branch", NULL};
    capture_t *output;
    git_repo *repo = new_git_repo();
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
                    if ((!git_repo_set_commit(repo, tmp + strlen(commit) + 1, GIT_HASH_LEN))) {
                        fputs("Error setting repo commit", stderr);
                        return;
                    }
                } else if ((tmp = strstr(*p, branch))) {
                    if ((!git_repo_set_branch(repo, tmp + strlen(branch) + 1, 0))) {
                        fputs("Error setting repo branch", stderr);
                        return;
                    }
                } else if ((tmp = strstr(*p, ab))) {
                    if ((!git_repo_set_ahead_behind(repo, tmp + strlen(ab) + 1))) return;
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
        log_debug("Repo results:\n"
                  "Commit:    %s\n"
                  "Branch:    %s\n"
                  "Changed:   %d\n"
                  "Untracked: %d\n"
                  "Ahead:     %d\n"
                  "Behind:    %d\n",
                  repo->commit, repo->branch, repo->changed, repo->untracked, repo->ahead,
                  repo->behind);
    } else {
        log_error("Error getting command output: %s", args[0]);
    }
    free_capture(output);
    free_git_repo(repo);
}

int main()
{
    options_t options = {
        .debug = 2,
        .format = NULL,
        .directory = NULL,
        .show_branch = 0,
        .show_revision = 0,
        .show_unknown = 0,
        .show_modified = 0,
    };
    set_options(&options);

    int log_level = 2 - options.debug;
    log_set_level(log_level);
#ifdef LOG_USE_COLOR
    log_debug("Color logging enabled!");
#endif
    parse_porcelain();

    // const char *s = "This is a test string %s %c buddy!";
    // log_debug("char[%zu]:'%s'\n", strlen(s), s);

    // for (; *s; s++) {
    //     if (*s == '%') {
    //         s++;
    //         switch (*s) {
    //         case 's':
    //             fputs("STRING", stdout);
    //             break;
    //         case 'c':
    //             fputs("CHAR", stdout);
    //             break;
    //         default:
    //             fprintf(stderr, "error: invalid format string token: %%%c\n", *s);
    //             exit(1);
    //         }
    //     } else {
    //         putchar(*s);
    //     }
    // }
    // putchar('\n');
}
