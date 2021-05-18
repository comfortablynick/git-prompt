#include "repo.h"
#include "log.h"
#include "util.h"
#include <stdlib.h>
#include <ctype.h>

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
