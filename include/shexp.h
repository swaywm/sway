#ifndef _SWAY_SHEXP_H
#define _SWAY_SHEXP_H

#include <stdbool.h>

/**
 * Takes a pointer to a string and reallocates it with the result of its shell
 * expansion. Undefined environment variables are considered as errors. Returns
 * true if it succeeds, otherwise returns false, leaving the original string
 * unchanged.
 */
bool shell_expand(char **path);

#endif
