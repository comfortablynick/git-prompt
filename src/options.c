#include "options.h"
#include <stdio.h>   // for sprintf, NULL
#include <stdlib.h>  // for free, calloc

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
