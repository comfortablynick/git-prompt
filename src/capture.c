#include "capture.h"
#include "log.h"        // for log_get_level, log_trace, log_debug, LOG_TRACE
#include <errno.h>      // for errno
#include <stdio.h>      // for NULL, size_t
#include <stdlib.h>     // for free, malloc, realloc, WEXITSTATUS, WIFEXITED
#include <string.h>     // for strcat, strerror
#include <sys/select.h> // for select, FD_ISSET, FD_SET, FD_ZERO, fd_set
#include <sys/wait.h>   // for waitpid
#include <unistd.h>     // for close, _exit, dup2, pipe, execvp, fork, read

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

static void free_capture(struct capture *self)
{
    if (self) {
        if (self->childout.buf) free(self->childout.buf);
        if (self->childerr.buf) free(self->childerr.buf);
        free(self);
    }
}

struct capture *new_capture()
{
    int bufsize = 4096;
    struct capture *result = malloc(sizeof(struct capture));
    if (!result) return NULL;
    result->free = free_capture;

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
