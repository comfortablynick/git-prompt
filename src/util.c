#include "util.h"
#include "log.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

static const struct options *_options = NULL;

void _options_debug(const struct options *options, char *buf)
{
    sprintf(buf,
            "Debug:         %d\n"
            "Format:        %s\n"
            "Directory:     %s\n"
            "Timeout:       %d\n"
            "Show branch:   %d\n"
            "Show commit:   %d\n"
            "Show unknown:  %d\n"
            "Show modified: %d",
            options->debug, options->format, options->directory, options->timeout,
            options->show_branch, options->show_commit, options->show_untracked,
            options->show_modified);
}

static void _options_set(const struct options *options) { _options = options; }

static void _options_free(struct options *options)
{
    if (options) {
        if (options->format) free(options->format);
        if (options->directory) free(options->directory);
        free(options);
    }
}

struct options *new_options()
{
    struct options *options = (struct options *)calloc(1, sizeof(struct options));
    options->set = _options_set;
    options->free = _options_free;
    options->sprint = _options_debug;
    return options;
}

static void init_dynbuf(struct dynbuf *dbuf, int bufsize)
{
    dbuf->size = bufsize;
    dbuf->len = 0;
    dbuf->buf = malloc(bufsize); // caller handles NULL
    dbuf->eof = 0;
}

static ssize_t read_dynbuf(int fd, struct dynbuf *dbuf)
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
        log_trace("capture: eof on fd %d; total read = %zu bytes", fd, dbuf->len);
        return 0;
    }
    log_trace("capture: read %zu bytes from child via fd %d", nread, fd);
    dbuf->len += nread;
    return nread;
}

void free_capture(struct capture *result)
{
    if (result) {
        if (result->childout.buf) free(result->childout.buf);
        if (result->childerr.buf) free(result->childerr.buf);
        free(result);
    }
}

struct capture *new_capture()
{
    int bufsize = 4096;
    struct capture *result = malloc(sizeof(struct capture));
    if (!result) return NULL;
    init_dynbuf(&result->childout, bufsize);
    if (!result->childout.buf) goto err;

    init_dynbuf(&result->childerr, bufsize);
    if (!result->childerr.buf) goto err;

    return result;

err:
    free_capture(result);
    return NULL;
}

struct capture *capture_child(char *const argv[])
{
    const char *file = *argv;
    if (log_get_level() >= LOG_TRACE) {
        char cmd_debug[256] = "";
        for (char *const *p = argv; *p; ++p) {
            strcat(cmd_debug, *p);
            strcat(cmd_debug, " \0");
        }
        log_trace("capture: %s", cmd_debug);
    }
    int stdout_pipe[] = {-1, -1};
    int stderr_pipe[] = {-1, -1};
    struct capture *result = NULL;
    if (pipe(stdout_pipe) < 0) goto err;
    if (pipe(stderr_pipe) < 0) goto err;

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
        log_error("error executing %s: %s", file, strerror(errno));
        _exit(127);
    }

    // parent: don't need write ends of the pipes
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);

    result = new_capture();
    if (!result) goto err;

    int cstdout = stdout_pipe[0];
    int cstderr = stderr_pipe[0];

    for (int done = 0; !done;) {
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
        log_debug("child process %s exited with status %d", file, result->status);
    if (result->signal != 0) log_warn("child process %s killed by signal %d", file, result->signal);
    if (result->childerr.len > 0)
        log_debug("child process %s wrote to stderr:%s", file, result->childerr.buf);
    return result;
err:
    if (stdout_pipe[0] > -1) close(stdout_pipe[0]);
    if (stdout_pipe[1] > -1) close(stdout_pipe[1]);
    if (stderr_pipe[0] > -1) close(stderr_pipe[0]);
    if (stderr_pipe[1] > -1) close(stderr_pipe[1]);
    free_capture(result);
    return NULL;
}

int strtoint_n(const char *str, int n)
{
    int nZ[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    int sign = 1;
    int place = 1;
    int ret = 0;

    for (int i = n - 1; i >= 0; --i, place *= 10) {
        int c = str[i];
        switch (c) {
        case 45: // '-'
            if (i == 0)
                sign = -1;
            else
                return -1;
            break;
        case 43: // '+'
            if (i == 0)
                sign = 1;
            else
                return -1;
            break;
        default:
            if (c >= 48 && c <= 57)
                ret += nZ[c - 48] * place;
            else
                return -1;
        }
    }
    return sign * ret;
}

char *str_dup(const char *str)
{
    const size_t len = strlen(str) + 1;
    char *p = malloc(len);
    return p ? memcpy(p, str, len) : NULL;
}

char *str_ndup(const char *str, const size_t n)
{
    // Use strnlen() if n > 0 to copy n chars
    const size_t len = n == 0 ? strlen(str) : strnlen(str, n);
    char *p = malloc(len + 1);
    if (!p) return NULL;
    p[len] = '\0';
    return memcpy(p, str, len);
}

char **str_split(const char *src, const char *delim, size_t *n)
{
    int i = 0;      // index
    int in = 0;     // in/out flag
    int nptrs = 10; // initial # of ptrs to allocate (must be > 0)
    char **dest = NULL;
    const char *p = src;
    const char *ep = p;

    // allocate ptrs
    if (!(dest = malloc(nptrs * sizeof *dest))) {
        perror("malloc-dest");
        return NULL;
    }
    *dest = NULL;

    for (;;) {
        if (!*ep || strchr(delim, *ep)) {
            size_t len = ep - p;
            if (in && len) {
                if (i == nptrs - 1) {
                    // realloc dest to temporary pointer/validate
                    void *tmp = realloc(dest, 2 * nptrs * sizeof *dest);
                    if (!tmp) {
                        perror("realloc-dest");
                        break; // don't exit, original dest still valid
                    }
                    dest = tmp; // assign reallocated block to dest
                    nptrs *= 2; // increment allocated pointer count
                }
                // allocate/validate storage for token
                if (!(dest[i] = malloc(len + 1))) {
                    perror("malloc-dest[i]");
                    break;
                }
                memcpy(dest[i], p, len); // copy len chars to storage
                dest[i++][len] = 0;      // nul-terminate, advance index
                dest[i] = NULL;          // set next pointer NULL
            }
            if (!*ep) // if at end, break
                break;
            in = 0;
        } else {             // normal word char
            if (!in) p = ep; // update start to end-pointer
            in = 1;
        }
        ep++;
    }
    if (n) *n = i;
    return dest;
}

size_t str_squish(char *str, bool trim)
{
    char *save = str;
    char *from = str;
    if (trim)
        while (isspace(*from)) ++from;
    for (; *from; ++from) {
        if (isspace(*from) && isspace(*(from - 1))) continue;
        *str++ = *from;
    }
    if (trim && isspace(*(str - 1))) --str;
    *str = '\0';
    return (str - save);
}
