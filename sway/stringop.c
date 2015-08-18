#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <ctype.h>
#include "stringop.h"
#include "string.h"
#include "list.h"

/* Note: This returns 8 characters for trimmed_start per tab character. */
void strip_whitespace(char *str) {
	int shift = 0;
	int bpair = 1;
	int in_str = 0, in_ch = 0;
	while (*str) {
		str[-shift] = str[0];
		if (*str == '"' && !in_ch) {
			in_str = !in_str;
		} else if (*str == '\'' && !in_str) {
			in_ch = !in_ch;
		} else if (!in_ch && !in_str) {
			if (isblank(*str)) {
				if (bpair) {
					++shift;
				}
				bpair=1;
			} else {
				bpair = 0;
			}
		}
		++str;
	}
	str[-shift-bpair] = 0;
}

void strip_comments(char *str) {
	int in_string = 0, in_character = 0;
	int i = 0;
	while (str[i] != '\0') {
		if (str[i] == '"' && !in_character) {
			in_string = !in_string;
		} else if (str[i] == '\'' && !in_string) {
			in_character = !in_character;
		} else if (!in_character && !in_string) {
			if (str[i] == '#' && i == 0) {
				str[i] = '\0';
				break;
			}
		}
		++i;
	}
}

list_t *split_string(const char *str, const char *delims) {
	list_t *res = create_list();
	int i, j;
	int len = strlen(str);
	for (i = 0, j = 0; i < len + 1; ++i) {
		if (strchr(delims, str[i]) || i == len) {
			if (i - j == 0) {
				continue;
			}
			char *left = malloc(i - j + 1);
			memcpy(left, str + j, i - j);
			left[i - j] = 0;
			list_add(res, left);
			j = i + 1;
			while (j <= len && str[j] && strchr(delims, str[j])) {
				j++;
				i++;
			}
		}
	}
	return res;
}

void free_flat_list(list_t *list) {
	int i;
	for (i = 0; i < list->length; ++i) {
		free(list->items[i]);
	}
	list_free(list);
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
			memmove(string + i, string + i + shift, len - i);
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
