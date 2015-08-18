#ifndef _SWAY_STRINGOP_H
#define _SWAY_STRINGOP_H
#include "list.h"

void strip_whitespace(char *str);
void strip_comments(char *str);
list_t *split_string(const char *str, const char *delims);
void free_flat_list(list_t *list);
char *code_strchr(const char *string, char delimiter);
char *code_strstr(const char *haystack, const char *needle);
int unescape_string(char *string);
char *join_args(char **argv, int argc);

#endif
