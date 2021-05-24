#include "test.h"
#include "repo.h"
#include "util.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void run_test(const char *name, struct git_repo *repo, const char *format, const char *expected)
{
    FILE *stream;
    char *buf;
    size_t buflen;
    stream = open_memstream(&buf, &buflen);
    parse_result(repo, format, stream);
    fclose(stream);
    buflen = str_squish(buf, true);
    printf("Test: %s\n------------------\n", name);
    printf("Result:    {buf=%s, len=%zu}\n"
           "Expected:  {buf=%s, len=%zu}\n"
           "Match:     %d\n\n",
           buf, buflen, expected, strlen(expected), (strcmp(buf, expected) == 0));
    assert((strcmp(buf, expected) == 0));
    free(buf);
}

void test_1()
{
    struct git_repo repo = {.branch = "test",
                            .commit = "abcd1234",
                            .ahead = 1,
                            .behind = 2,
                            .changed = 0,
                            .untracked = 100};
    const char *format = "  %b@%c %m%M %u%U %a%A%z%Z  ";
    const char *expected = "test@abcd1234 …100 ↑1↓2";
    run_test("Test 1", &repo, format, expected);
}

void test_2()
{
    struct git_repo repo = {.branch = "main",
                            .commit = "abcd1234",
                            .ahead = 0,
                            .behind = 0,
                            .changed = 30,
                            .untracked = 0};
    const char *format = "  %b@%c %m%M %u%U %a%A%z%Z  ";
    const char *expected = "main@abcd1234 *30";
    run_test("Test 2", &repo, format, expected);
}

void run_tests() {
    test_1();
    test_2();
}
