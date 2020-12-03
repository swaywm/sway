#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wordexp.h>
#include "list.h"
#include "log.h"
#include "stringop.h"

static const char whitespace[] = " \f\n\r\t\v";

void strip_whitespace(char *str) {
	size_t len = strlen(str);
	size_t start = strspn(str, whitespace);
	memmove(str, &str[start], len + 1 - start);

	if (*str) {
		for (len -= start + 1; isspace(str[len]); --len) {}
		str[len + 1] = '\0';
	}
}

void strip_quotes(char *str) {
	bool in_str = false;
	bool in_chr = false;
	bool escaped = false;
	char *end = strchr(str,0);
	while (*str) {
		if (*str == '\'' && !in_str && !escaped) {
			in_chr = !in_chr;
			goto shift_over;
		} else if (*str == '\"' && !in_chr && !escaped) {
			in_str = !in_str;
			goto shift_over;
		} else if (*str == '\\') {
			escaped = !escaped;
			++str;
			continue;
		}
		escaped = false;
		++str;
		continue;
		shift_over:
		memmove(str, str+1, end-- - str);
	}
	*end = '\0';
}

char *lenient_strcat(char *dest, const char *src) {
	if (dest && src) {
		return strcat(dest, src);
	}
	return dest;
}

char *lenient_strncat(char *dest, const char *src, size_t len) {
	if (dest && src) {
		return strncat(dest, src, len);
	}
	return dest;
}

// strcmp that also handles null pointers.
int lenient_strcmp(const char *a, const char *b) {
	if (a == b) {
		return 0;
	} else if (!a) {
		return -1;
	} else if (!b) {
		return 1;
	} else {
		return strcmp(a, b);
	}
}

list_t *split_string(const char *str, const char *delims) {
	list_t *res = create_list();
	char *copy = strdup(str);

	char *token = strtok(copy, delims);
	while (token) {
		list_add(res, strdup(token));
		token = strtok(NULL, delims);
	}
	free(copy);
	return res;
}

char **split_args(const char *start, int *argc) {
	*argc = 0;
	int alloc = 2;
	char **argv = malloc(sizeof(char *) * alloc);
	bool in_token = false;
	bool in_string = false;
	bool in_char = false;
	bool in_brackets = false; // brackets are used for criteria
	bool escaped = false;
	const char *end = start;
	if (start) {
		while (*start) {
			if (!in_token) {
				start = (end += strspn(end, whitespace));
				in_token = true;
			}
			if (*end == '"' && !in_char && !escaped) {
				in_string = !in_string;
			} else if (*end == '\'' && !in_string && !escaped) {
				in_char = !in_char;
			} else if (*end == '[' && !in_string && !in_char && !in_brackets && !escaped) {
				in_brackets = true;
			} else if (*end == ']' && !in_string && !in_char && in_brackets && !escaped) {
				in_brackets = false;
			} else if (*end == '\\') {
				escaped = !escaped;
			} else if (*end == '\0' || (!in_string && !in_char && !in_brackets
						&& !escaped && strchr(whitespace, *end))) {
				goto add_token;
			}
			if (*end != '\\') {
				escaped = false;
			}
			++end;
			continue;
			add_token:
			if (end - start > 0) {
				char *token = malloc(end - start + 1);
				strncpy(token, start, end - start + 1);
				token[end - start] = '\0';
				argv[*argc] = token;
				if (++*argc + 1 == alloc) {
					argv = realloc(argv, (alloc *= 2) * sizeof(char *));
				}
			}
			in_token = false;
			escaped = false;
		}
	}
	argv[*argc] = NULL;
	return argv;
}

void free_argv(int argc, char **argv) {
	while (argc-- > 0) {
		free(argv[argc]);
	}
	free(argv);
}

int unescape_string(char *string) {
	/* TODO: More C string escapes */
	int len = strlen(string);
	int i;
	for (i = 0; string[i]; ++i) {
		if (string[i] == '\\') {
			switch (string[++i]) {
			case '0':
				string[i - 1] = '\0';
				return i - 1;
			case 'a':
				string[i - 1] = '\a';
				string[i] = '\0';
				break;
			case 'b':
				string[i - 1] = '\b';
				string[i] = '\0';
				break;
			case 'f':
				string[i - 1] = '\f';
				string[i] = '\0';
				break;
			case 'n':
				string[i - 1] = '\n';
				string[i] = '\0';
				break;
			case 'r':
				string[i - 1] = '\r';
				string[i] = '\0';
				break;
			case 't':
				string[i - 1] = '\t';
				string[i] = '\0';
				break;
			case 'v':
				string[i - 1] = '\v';
				string[i] = '\0';
				break;
			case '\\':
				string[i] = '\0';
				break;
			case '\'':
				string[i - 1] = '\'';
				string[i] = '\0';
				break;
			case '\"':
				string[i - 1] = '\"';
				string[i] = '\0';
				break;
			case '?':
				string[i - 1] = '?';
				string[i] = '\0';
				break;
			case 'x':
				{
					unsigned char c = 0;
					if (string[i+1] >= '0' && string[i+1] <= '9') {
						c = string[i+1] - '0';
						if (string[i+2] >= '0' && string[i+2] <= '9') {
							c *= 0x10;
							c += string[i+2] - '0';
							string[i+2] = '\0';
						}
						string[i+1] = '\0';
					}
					string[i] = '\0';
					string[i - 1] = c;
				}
			}
		}
	}
	// Shift characters over nullspaces
	int shift = 0;
	for (i = 0; i < len; ++i) {
		if (string[i] == 0) {
			shift++;
			continue;
		}
		string[i-shift] = string[i];
	}
	string[len - shift] = 0;
	return len - shift;
}

char *join_args(char **argv, int argc) {
	if (!sway_assert(argc > 0, "argc should be positive")) {
		return NULL;
	}
	int len = 0, i;
	for (i = 0; i < argc; ++i) {
		len += strlen(argv[i]) + 1;
	}
	char *res = malloc(len);
	len = 0;
	for (i = 0; i < argc; ++i) {
		strcpy(res + len, argv[i]);
		len += strlen(argv[i]);
		res[len++] = ' ';
	}
	res[len - 1] = '\0';
	return res;
}

static inline char *argsep_next_interesting(const char *src, const char *delim) {
	char *special = strpbrk(src, "\"'\\");
	char *next_delim = strpbrk(src, delim);
	if (!special) {
		return next_delim;
	}
	if (!next_delim) {
		return special;
	}
	return (next_delim < special) ? next_delim : special;
}

char *argsep(char **stringp, const char *delim, char *matched) {
	char *start = *stringp;
	char *end = start;
	bool in_string = false;
	bool in_char = false;
	bool escaped = false;
	char *interesting = NULL;

	while ((interesting = argsep_next_interesting(end, delim))) {
		if (escaped && interesting != end) {
			escaped = false;
		}
		if (*interesting == '"' && !in_char && !escaped) {
			in_string = !in_string;
			end = interesting + 1;
		} else if (*interesting == '\'' && !in_string && !escaped) {
			in_char = !in_char;
			end = interesting + 1;
		} else if (*interesting == '\\') {
			escaped = !escaped;
			end = interesting + 1;
		} else if (!in_string && !in_char && !escaped) {
			// We must have matched a separator
			end = interesting;
			if (matched) {
				*matched = *end;
			}
			if (end - start) {
				*(end++) = 0;
				*stringp = end;
				break;
			} else {
				end = ++start;
			}
		} else {
			end++;
		}
	}
	if (!interesting) {
		*stringp = NULL;
		if (matched) {
			*matched = '\0';
		}
	}
	return start;
}

bool expand_path(char **path) {
	wordexp_t p = {0};
	while (strstr(*path, "  ")) {
		*path = realloc(*path, strlen(*path) + 2);
		char *ptr = strstr(*path, "  ") + 1;
		memmove(ptr + 1, ptr, strlen(ptr) + 1);
		*ptr = '\\';
	}
	if (wordexp(*path, &p, 0) != 0 || p.we_wordv[0] == NULL) {
		wordfree(&p);
		return false;
	}
	free(*path);
	*path = join_args(p.we_wordv, p.we_wordc);
	wordfree(&p);
	return true;
}
