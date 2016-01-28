#ifndef _SWAY_READLINE_H
#define _SWAY_READLINE_H

#include <stdio.h>

char *read_line(FILE *file);
char *read_line_buffer(FILE *file, char *string, size_t string_len);

#endif
