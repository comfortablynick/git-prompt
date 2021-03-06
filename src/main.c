#include "log.h"              // for log_set_quiet, log_set_level, LOG_DEBUG
#include "options.h"          // for options, new_options
#include "repo.h"             // for new_git_repo, parse_porcelain, parse_r...
#include "test.h"             // for test_parse
#include "util.h"             // for str_ndup, str_squish
#include <bits/getopt_core.h> // for getopt, optarg, optind
#include <libgen.h>           // for basename
#include <stdbool.h>          // for true, false
#include <stdio.h>            // for fprintf, NULL, fclose, fputs, open_mem...
#include <stdlib.h>           // for exit, free, getenv, realpath, strtol
#include <unistd.h>           // for getcwd

#ifndef FMT_STRING
#define FMT_STRING "%b@%c"
#endif

/// Parse cli arguments into options struct
struct options *parse_args(int argc, char **argv)
{
    struct options *options = new_options();
    if (!options) return NULL;
    int opt;
    while ((opt = getopt(argc, argv, "hqvTt:f:")) != -1) {
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
        case 't':
            options->timeout = strtol(optarg, NULL, 10);
            break;
        case 'T':
            run_tests();
            exit(EXIT_SUCCESS);
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
                    "  -T   run internal tests\n"
                    "  -f   tokenized string that determines output\n"
                    "       %b  show branch\n"
                    "       %c  show commit hash\n"
                    "       %u  indicate unknown (untracked) files with '?'\n"
                    "       %U  show count of unknown files\n"
                    "       %m  indicate uncommitted changes with '*'\n"
                    "       %M  show count of uncommitted changes\n"
                    "       %a  indicate unpushed changes with '^'\n"
                    "       %A  show count of unpushed changes\n"
                    "       %%  show '%'\n"
                    "  dir  location of git repo (default is cwd)\n"
                    "\nEnvironment:\n"
                    "  $GITPROMPT_FORMAT  format string");
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

    // Write result to buffer and print all at once
    FILE *stream;
    char *buf;
    size_t buflen;
    stream = open_memstream(&buf, &buflen);
    parse_result(repo, options->format, stream);
    fclose(stream);
    str_squish(buf, true);
    fputs(buf, stdout);
    options->free(options);
    repo->free(repo);
    free(buf);
}
