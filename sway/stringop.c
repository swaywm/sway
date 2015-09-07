#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <ctype.h>
#include "stringop.h"
#include "log.h"
#include "string.h"
#include "list.h"

const char *whitespace = " \f\n\r\t\v";

/* Note: This returns 8 characters for trimmed_start per tab character. */
char *strip_whitespace(char *_str) {
	if (*_str == '\0')
		return _str;
	char *strold = _str;
	while (*_str == ' ' || *_str == '\t') {
		_str++;
	}
	char *str = malloc(strlen(_str) + 1);
	strcpy(str, _str);
	free(strold);
	int i;
	for (i = 0; str[i] != '\0'; ++i);
	do {
		i--;
	} while (i >= 0 && (str[i] == ' ' || str[i] == '\t')); 
	str[i + 1] = '\0';
	return str;
}

char *strip_comments(char *str) {
	int in_string = 0, in_character = 0;
	int i = 0;
	while (str[i] != '\0') {
		if (str[i] == '"' && !in_character) {
			in_string = !in_string;
		} else if (str[i] == '\'' && !in_string) {
			in_character = !in_character;
		} else if (!in_character && !in_string) {
			if (str[i] == '#') {
				str[i] = '\0';
				break;
			}
		}
		++i;
	}
	return str;
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

list_t *split_string(const char *str, const char *delims) {
	list_t *res = create_list();
	char *copy = malloc(strlen(str) + 1);
	char *token;
	strcpy(copy, str);

	token = strtok(copy, delims);
	while(token) {
		token = strdup(token);
		list_add(res, token);
		token = strtok(NULL, delims);
	}
	free(copy);
	return res;
}

void free_flat_list(list_t *list) {
	int i;
	for (i = 0; i < list->length; ++i) {
		free(list->items[i]);
	}
	list_free(list);
}

char **split_args(const char *start, int *argc) {
	*argc = 0;
	int alloc = 2;
	char **parts = malloc(sizeof(char *) * alloc);
	bool in_token = false;
	bool in_string = false;
	bool in_char = false;
	bool escaped = false;
	const char *end = start;
	while (*start) {
		if (!in_token) {
			start = (end += strspn(end, whitespace));
			in_token = true;
		}
		if (*end == '"' && !in_char && !escaped) {
			in_string = !in_string;
		} else if (*end == '\'' && !in_string && !escaped) {
			in_char = !in_char;
		} else if (*end == '\\') {
			escaped = !escaped;
		} else if (*end == '\0' || (!in_string && !in_char && !escaped
				&& strchr(whitespace, *end))) {
			goto add_part;
		}
		if (*end != '\\') {
			escaped = false;
		}
		++end;
		continue;
		add_part:
		if (end - start > 0) {
			char *token = malloc(end - start + 1);
			strncpy(token, start, end - start + 1);
			token[end - start] = '\0';
			strip_quotes(token);
			unescape_string(token);
			parts[*argc] = token;
			if (++*argc == alloc) {
				parts = realloc(parts, (alloc *= 2) * sizeof(char *));
			}
		}
		in_token = false;
		escaped = false;
	}
	return parts;
}

void free_argv(int argc, char **argv) {
	while (--argc) {
		free(argv[argc]);
	}
	free(argv);
}

char *code_strstr(const char *haystack, const char *needle) {
	/* TODO */
	return strstr(haystack, needle);
}

char *code_strchr(const char *str, char delimiter) {
	int in_string = 0, in_character = 0;
	int i = 0;
	while (str[i] != '\0') {
		if (str[i] == '"' && !in_character) {
			in_string = !in_string;
		} else if (str[i] == '\'' && !in_string) {
			in_character = !in_character;
		} else if (!in_character && !in_string) {
			if (str[i] == delimiter) {
				return (char *)str + i;
			}
		}
		++i;
	}
	return NULL;
}

int unescape_string(char *string) {
	/* TODO: More C string escapes */
	int len = strlen(string);
	int i;
	for (i = 0; string[i]; ++i) {
		if (string[i] == '\\') {
			--len;
			int shift = 0;
			switch (string[++i]) {
			case '0':
				string[i - 1] = '\0';
				shift = 1;
				break;
			case 'a':
				string[i - 1] = '\a';
				shift = 1;
				break;
			case 'b':
				string[i - 1] = '\b';
				shift = 1;
				break;
			case 'f':
				string[i - 1] = '\f';
				shift = 1;
				break;
			case 'n':
				string[i - 1] = '\n';
				shift = 1;
				break;
			case 'r':
				string[i - 1] = '\r';
				shift = 1;
				break;
			case 't':
				string[i - 1] = '\t';
				shift = 1;
				break;
			case 'v':
				string[i - 1] = '\v';
				shift = 1;
				break;
			case '\\':
				shift = 1;
				break;
			case '\'':
				string[i - 1] = '\'';
				shift = 1;
				break;
			case '\"':
				string[i - 1] = '\"';
				shift = 1;
				break;
			case '?':
				string[i - 1] = '?';
				shift = 1;
				break;
			case 'x':
				{
					unsigned char c = 0;
					shift = 1;
					if (string[i+1] >= '0' && string[i+1] <= '9') {
						shift = 2;
						c = string[i+1] - '0';
						if (string[i+2] >= '0' && string[i+2] <= '9') {
							shift = 3;
							c *= 0x10;
							c += string[i+2] - '0';
						}
					}
					string[i - 1] = c;
				}
			}
			memmove(string + i, string + i + shift, len - i + 1);
		}
	}
	return len;
}

char *join_args(char **argv, int argc) {
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

/*
 * Join a list of strings, adding separator in between. Separator can be NULL.
 */
char *join_list(list_t *list, char *separator) {
	if (!sway_assert(list != NULL, "list != NULL") || list->length == 0) {
		return NULL;
	}

	size_t len = 1; // NULL terminator
	size_t sep_len = 0;
	if (separator != NULL) {
		sep_len = strlen(separator);
		len += (list->length - 1) * sep_len;
	}

	for (int i = 0; i < list->length; i++) {
		len += strlen(list->items[i]);
	}

	char *res = malloc(len);

	char *p = res + strlen(list->items[0]);
	strcpy(res, list->items[0]);

	for (int i = 1; i < list->length; i++) {
		if (sep_len) {
			memcpy(p, separator, sep_len);
			p += sep_len;
		}
		strcpy(p, list->items[i]);
		p += strlen(list->items[i]);
	}

	*p = '\0';

	return res;
}
