#include "test.h"
#include "repo.h"
#include "util.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// Set up repo struct and test result of parse function
void test_parse()
{
    struct git_repo repo = {.branch = "test",
                            .commit = "abcd1234",
                            .ahead = 1,
                            .behind = 2,
                            .changed = 0,
                            .untracked = 100};
    const char *format = "  %b@%c %m%M %u%U %a%A%z%Z  ";
    const char *expected = "test@abcd1234 ?100 ↑1↓2";
    FILE *stream;
    char *buf;
    size_t buflen;
    stream = open_memstream(&buf, &buflen);
    parse_result(&repo, format, stream);
    fclose(stream);
    buflen = str_squish(buf, true);
    puts("Test results");
    puts("------------");
    printf("Test 1\n"
           "    Result:    {buf=%s, len=%zu}\n"
           "    Expected:  {buf=%s, len=%zu}\n"
           "    Match:     %d\n",
           buf, buflen, expected, strlen(expected), (strcmp(buf, expected) == 0));
    assert((strcmp(buf, expected) == 0));
    free(buf);
}
