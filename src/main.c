#define DEBUG 1
#define GIT_HASH_LEN 7

#include <libgen.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "util.h"

#define debug(fmt, ...)                                                                            \
    do {                                                                                           \
        if (DEBUG)                                                                                 \
            fprintf(stderr, "DEBUG:%s:%d:%s(): " fmt, basename(__FILE__), __LINE__, __func__,      \
                    ##__VA_ARGS__);                                                                \
    } while (0)

typedef struct dynbuf
{
    size_t size; // bytes allocated
    size_t len;  // bytes filled
    char *buf;
    int eof;
} dynbuf;

typedef struct capture_t
{
    dynbuf childout;
    dynbuf childerr;
    int status; // exit status that child passed (if any)
    int signal; // signal that killed the child (if any)
} capture_t;

static void init_dynbuf(dynbuf *dbuf, int bufsize)
{
    dbuf->size = bufsize;
    dbuf->len = 0;
    dbuf->buf = malloc(bufsize); // caller handles NULL
    dbuf->eof = 0;
}

static ssize_t read_dynbuf(int fd, dynbuf *dbuf)
{
    size_t avail = dbuf->size - dbuf->len;
    if (avail < 1024) {
        dbuf->size *= 2;
        dbuf->buf = realloc(dbuf->buf, dbuf->size);
        avail = dbuf->size - dbuf->len;
    }
    // read avail-1 bytes to leave room for termininating NULL
    ssize_t nread = read(fd, dbuf->buf + dbuf->len, avail - 1);
    if (nread < 0)
        return nread;
    else if (nread == 0) {
        dbuf->buf[dbuf->len] = '\0';
        dbuf->eof = 1;
        debug("capture: eof on fd %d; total read = %zu bytes\n", fd, dbuf->len);
        return 0;
    }
    debug("capture: read %zu bytes from child via fd %d\n", nread, fd);
    dbuf->len += nread;
    return nread;
}

void free_capture(capture_t *result)
{
    if (result) {
        if (result->childout.buf) free(result->childout.buf);
        if (result->childerr.buf) free(result->childerr.buf);
        free(result);
    }
}

capture_t *new_capture()
{
    int bufsize = 4096;
    capture_t *result = malloc(sizeof(capture_t));
    if (result == NULL) goto err;
    init_dynbuf(&result->childout, bufsize);
    if (!result->childout.buf) goto err;

    init_dynbuf(&result->childerr, bufsize);
    if (!result->childerr.buf) goto err;

    return result;

err:
    free_capture(result);
    return NULL;
}

capture_t *capture_child(char *const argv[])
{
    const char *file = *argv;
    int stdout_pipe[] = {-1, -1};
    int stderr_pipe[] = {-1, -1};
    capture_t *result = NULL;
    if (pipe(stdout_pipe) < 0) goto err;
    if (pipe(stderr_pipe) < 0) goto err;

    // for(; *argv; ++argv) puts(*argv);
    pid_t pid = fork();
    if (pid < 0) {
        goto err;
    }
    if (pid == 0) {            // in the child
        close(stdout_pipe[0]); // don't need the read ends of the pipes
        close(stderr_pipe[0]);
        if (dup2(stdout_pipe[1], STDOUT_FILENO) < 0) _exit(1);
        if (dup2(stderr_pipe[1], STDERR_FILENO) < 0) _exit(1);

        execvp(file, argv);
        debug("error executing %s: %s\n", file, strerror(errno));
        _exit(127);
    }

    // parent: don't need write ends of the pipes
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    result = new_capture();
    if (!result) goto err;

    int cstdout = stdout_pipe[0];
    int cstderr = stderr_pipe[0];

    int done = 0;
    while (!done) {
        int maxfd = -1;
        fd_set child_fds;
        FD_ZERO(&child_fds);
        if (!result->childout.eof) {
            FD_SET(cstdout, &child_fds);
            maxfd = cstdout;
        }
        if (!result->childerr.eof) {
            FD_SET(cstderr, &child_fds);
            maxfd = cstderr;
        }
        int numavail = select(maxfd + 1, &child_fds, NULL, NULL, NULL);
        if (numavail < 0)
            goto err;
        else if (numavail == 0) // EOF on both pipes
            break;

        if (FD_ISSET(cstdout, &child_fds)) {
            if (read_dynbuf(cstdout, &result->childout) < 0) goto err;
        }
        if (FD_ISSET(cstderr, &child_fds)) {
            if (read_dynbuf(cstderr, &result->childerr) < 0) goto err;
        }
        done = result->childout.eof && result->childerr.eof;
    }

    int status;
    waitpid(pid, &status, 0);
    result->status = result->signal = 0;
    if (WIFEXITED(status))
        result->status = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
        result->signal = WTERMSIG(status);

    if (result->status != 0)
        debug("child process %s exited with status %d\n", file, result->status);
    if (result->signal != 0) debug("child process %s killed by signal %d\n", file, result->signal);
    if (result->childerr.len > 0)
        debug("child process %s wrote to stderr:\n%s", file, result->childerr.buf);
    return result;
err:
    if (stdout_pipe[0] > -1) close(stdout_pipe[0]);
    if (stdout_pipe[1] > -1) close(stdout_pipe[1]);
    if (stderr_pipe[0] > -1) close(stderr_pipe[0]);
    if (stderr_pipe[1] > -1) close(stderr_pipe[1]);
    free_capture(result);
    return NULL;
}

typedef struct git_repo
{
    char *branch;
    char *commit;
    int untracked;
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

void parse_porcelain()
{
    char *args[] = {"git", "status", "--porcelain=2", "--untracked-files=normal", "--branch", NULL};
    capture_t *output;
    git_repo *repo = new_git_repo();
    if ((output = capture_child(args))) {
        char *cstdout = output->childout.buf;
        char **split;
        int line = 1;
        if ((split = str_split(cstdout, "\n"))) {
            for (char **p = split; *p; ++p) {
                debug("L%d: %s\n", line, *p);
                char *tmp;
                if ((tmp = strstr(*p, "branch.oid"))) {
                    tmp = tmp + 11;
                    // Only copy first GIT_HASH_LEN chars
                    git_repo_set_commit(repo, tmp, GIT_HASH_LEN);
                } else if ((tmp = strstr(*p, "branch.head"))) {
                    tmp = tmp + 12;
                    git_repo_set_branch(repo, tmp, 0);
                }
                free(*p);
                ++line;
            }
            free(split);
        }
        puts("======== Results ========");
        printf("Commit:    %s\n", repo->commit);
        printf("Branch:    %s\n", repo->branch);
        puts("======= End Results =====\n");
    } else {
        fputs("Error getting command output", stderr);
    }
    free_capture(output);
    free_git_repo(repo);
}

int main()
{
    parse_porcelain();

    // const char *s = "This is a test string %s %c buddy!";
    // debug("char[%zu]:'%s'\n", strlen(s), s);

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