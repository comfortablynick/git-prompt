#include "repo.h"
#include "log.h"
#include "util.h"
#include "options.h"
#include "capture.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/// Completely free git_repo struct
static void git_repo_free(struct git_repo *self)
{
    if (!self) return;
    if (self->branch) free(self->branch);
    if (self->commit) free(self->commit);
    free(self);
}

/// Set buf to debug repr of git_repo
static void git_repo_debug(const struct git_repo *self, char *buf)
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
static int git_repo_set_branch(struct git_repo *self, const char *branch, size_t len)
{
    if (self->branch) free(self->branch);
    self->branch = str_ndup(branch, len);
    return !!self->branch;
}

/// Set branch name in git_repo struct
static int git_repo_set_commit(struct git_repo *self, const char *commit, size_t len)
{
    if (self->commit) free(self->commit);
    self->commit = str_ndup(commit, len);
    return !!self->commit;
}

/// Set number of commits ahead/behind upstream
static int git_repo_set_ahead_behind(struct git_repo *self, char *buf)
{
    log_debug("Ahead/behind: %s", buf);
    int found = 0;
    while (*buf) {
        if (isdigit(*buf)) {
            int val = (int)strtol(buf, &buf, 10);
            if (found == 0) {
                self->ahead = val;
            } else {
                self->behind = val;
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
    repo->sprint = git_repo_debug;
    repo->free = git_repo_free;
    repo->set_branch = git_repo_set_branch;
    repo->set_commit = git_repo_set_commit;
    repo->set_ahead_behind = git_repo_set_ahead_behind;
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
    output->free(output);
}
