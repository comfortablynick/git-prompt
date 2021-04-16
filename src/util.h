#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

char *str_dup(const char *str);
char *str_ndup(const char *str, size_t n);
char **str_split(const char *src, const char *delim);
