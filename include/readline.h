#ifndef _SWAY_READLINE_H
#define _SWAY_READLINE_H

#include <stdio.h>

char *read_line(FILE *file);
char *peek_line(FILE *file, int line_offset, long *position);
char *read_line_buffer(FILE *file, char *string, size_t string_len);

#endif
