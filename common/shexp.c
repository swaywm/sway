#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <wordexp.h>
#include "shexp.h"

static const char escaped_quote[] = "'\"'\"'";

static char *escape(const char *str) {
	size_t escaped_len = 2; // quotes at the begining and end
	for (size_t i = 0; str[i] != '\0'; ++i) {
		if (str[i] == '\'') {
			escaped_len += strlen(escaped_quote);
		} else {
			++escaped_len;
		}
	}

	char *escaped = malloc(escaped_len + 1);
	escaped[0] = '\'';
	size_t j = 1;
	for (size_t i = 0; str[i] != '\0'; ++i) {
		if (str[i] == '\'') {
			memcpy(&escaped[j], escaped_quote, strlen(escaped_quote));
			j += strlen(escaped_quote);
		} else {
			escaped[j] = str[i];
			++j;
		}
	}
	escaped[j] = '\'';
	escaped[j + 1] = '\0';

	return escaped;
}

bool shell_expand(char **path) {
	char *escaped_path = escape(*path);

	wordexp_t p;
	bool ret = false;
	if (wordexp(escaped_path, &p, WRDE_UNDEF) == 0) {
		free(*path);
		*path = strdup(p.we_wordv[0]);
		wordfree(&p);
		ret = true;
	}

	free(escaped_path);
	return ret;
}
