#pragma once

#include <stdbool.h> // for bool
#include <stdio.h>   // for size_t

/// Alternative to strtol which allows '+' and '-' prefixes
int strtoint_n(const char *str, int n);

/// Allocate new memory and copy string
char *str_dup(const char *str);

/// Allocate new memory and copy string, like `strndup()`
///
/// Operates like `strdup()` if `n == 0`
char *str_ndup(const char *str, size_t n);

/// Split `src` into tokens with sentinel `NULL` after last token.
///
/// Return allocated pointer-to-pointer with sentinel `NULL` on success,
/// or `NULL` on failure to allocate initial block of pointers. The number
/// of allocated pointers are doubled each time reallocation required.
///
/// Set `n` to the size of pointer-to-pointer array if `n != NULL`
/// Based on: https://stackoverflow.com/a/60409814
char **str_split(const char *src, const char *delim, size_t *n);

/// Collapse whitespace in char array in place and return new size
///
/// Also removes leading whitespace if trim is true
size_t str_squish(char *str, bool trim);
