#define BUFSIZE 128
#define DEBUG 1

#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define debug(fmt, ...)                                                                            \
    do {                                                                                           \
        if (DEBUG)                                                                                 \
            fprintf(stderr, "DEBUG:%s:%d:%s(): " fmt, basename(__FILE__), __LINE__, __func__,      \
                    ##__VA_ARGS__);                                                                \
    } while (0)

void put(const char *str)
{
    while (*str) putchar(*str++);
}

size_t slen(const char *str)
{
    size_t len = 0;
    for (; str[len]; ++len)
        ;
    return len;
}

char *_strdup(const char *str)
{
    size_t len = strlen(str) + 1;
    char *p = malloc(len);
    return p ? memcpy(p, str, len) : NULL;
}

char *ft_strdup(const char *str) { return strcpy(malloc(strlen(str) + 1), str); }

int parse_output(const char *cmd)
{
    char buf[BUFSIZE];
    FILE *fp;

    if ((fp = popen(cmd, "r")) == NULL) {
        printf("Error opening pipe!\n");
        return -1;
    }

    while (fgets(buf, BUFSIZE, fp) != NULL) {
        // Do whatever you want here...
        printf("OUTPUT: %s", buf);
    }

    if (pclose(fp)) {
        printf("Command not found or exited with error status\n");
        return -1;
    }

    return 0;
}

char *get_output(char **argv)
{
    FILE *fd;
    fd = popen(argv[0], "r");
    if (!fd) return NULL;

    char buffer[256];
    size_t chread;
    /* String to store entire command contents in */
    size_t comalloc = 256;
    size_t comlen = 0;
    char *comout = malloc(comalloc);

    /* Use fread so binary data is dealt with correctly */
    while ((chread = fread(buffer, 1, sizeof(buffer), fd)) != 0) {
        if (comlen + chread >= comalloc) {
            comalloc *= 2;
            comout = realloc(comout, comalloc);
        }
        memmove(comout + comlen, buffer, chread);
        comlen += chread;
    }

    /* We can now work with the output as we please. Just print
     * out to confirm output is as expected */
    // fwrite(comout, 1, comlen, stdout);
    // free(comout);
    pclose(fd);
    // return 0;
    return comout;
}

typedef struct // dynbuf
{
    size_t size; // bytes allocated
    size_t len;  // bytes filled
    char *buf;
    int eof;
} dynbuf;

typedef struct // capture_t
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
        // debug("capture: eof on fd %d; total read = %d bytes", fd, dbuf->len);
        return 0;
    }
    // debug("capture: read %d bytes from child via fd %d", nread, fd);
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
        // debug("error executing %s: %s\n", file, strerror(errno));
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

/* Split src into tokens with sentinel NULL after last token.
 * return allocated pointer-to-pointer with sentinel NULL on success,
 * or NULL on failure to allocate initial block of pointers. The number
 * of allocated pointers are doubled each time reallocation required.
 */
char **strsplit(const char *src, const char *delim)
{
    int i = 0;     // index
    int in = 0;    // in/out flag
    int nptrs = 2; // initial # of ptrs to allocate (must be > 0)
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
    return dest;
}

void parse_porcelain()
{
    char *args[] = {"git", "status", "--porcelain=2", "--untracked-files=normal", "--branch", NULL};
    capture_t *output = capture_child(args);
    if (!output) fputs("Error getting command output", stderr);
    char *cstdout = output->childout.buf;
    char **split;
    int line = 1;
    if ((split = strsplit(cstdout, "\n"))) {
        for (char **p = split; *p; ++p) {
            printf("L%d: %s\n", line, *p);
            free(*p);
            ++line;
        }
        free(split);
    }
    // int i = 0;
    // for (char *c = cstdout; *c; ++c) {
    //     if (c == cstdout || *(c - 1) == '\n') {
    //         ++i;
    //         // beg of line
    //         printf("Line %d: ", i);
    //     }
    //     putchar(*c);
    // }
    free_capture(output);
}

int main()
{
    const char *s = "This is a test string %s %c buddy!";
    printf("slen result:      %zu\n", slen(s));
    printf("strlen result:    %zu\n", strlen(s));

    char *scpy = ft_strdup(s);
    puts(scpy);
    free(scpy);

    parse_porcelain();

    for (; *s; s++) {
        if (*s == '%') {
            s++;
            switch (*s) {
            case 's':
                put("STRING");
                break;
            case 'c':
                put("CHAR");
                break;
            default:
                fprintf(stderr, "error: invalid format string token: %%%c\n", *s);
                exit(1);
            }
        } else {
            putchar(*s);
        }
    }
}
