#ifndef _SWAY_STRINGOP_H
#define _SWAY_STRINGOP_H
#include "list.h"

// array of whitespace characters to use for delims
extern const char *whitespace;

char *strip_whitespace(char *str);
char *strip_comments(char *str);

// Simply split a string with delims, free with `free_flat_list`
list_t *split_string(const char *str, const char *delims);
void free_flat_list(list_t *list);

// Splits an argument string, keeping quotes intact
char **split_args(const char *str, int *argc);
void free_argv(int argc, char **argv);

char *code_strchr(const char *string, char delimiter);
char *code_strstr(const char *haystack, const char *needle);
int unescape_string(char *string);
char *join_args(char **argv, int argc);
char *join_list(list_t *list, char *separator);

char *strdup(const char *);
#endif
