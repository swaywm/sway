#include "stringop.h"
#include <stdlib.h>
#include <stdio.h>
#include "string.h"
#include "list.h"
#include <strings.h>

/* Note: This returns 8 characters for trimmed_start per tab character. */
char *strip_whitespace(char *_str, int *trimmed_start) {
	*trimmed_start = 0;
	if (*_str == '\0')
		return _str;
	char *strold = _str;
	while (*_str == ' ' || *_str == '\t') {
		if (*_str == '\t') {
			*trimmed_start += 8;
		} else {
			*trimmed_start += 1;
		}
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
			if (str[i] == '#' && i == 0) {
				str[i] = '\0';
				break;
			}
		}
		++i;
	}
	return str;
}

list_t *split_string(const char *str, const char *delims) {
	list_t *res = create_list();
	int i, j;
	for (i = 0, j = 0; i < strlen(str) + 1; ++i) {
		if (strchr(delims, str[i]) || i == strlen(str)) {
			if (i - j == 0) {
				continue;
			}
			char *left = malloc(i - j + 1);
			memcpy(left, str + j, i - j);
			left[i - j] = 0;
			list_add(res, left);
			j = i + 1;
			while (j <= strlen(str) && str[j] && strchr(delims, str[j])) {
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
			switch (string[++i]) {
			case '0':
				string[i - 1] = '\0';
				memmove(string + i, string + i + 1, len - i);
				break;
			case 'a':
				string[i - 1] = '\a';
				memmove(string + i, string + i + 1, len - i);
				break;
			case 'b':
				string[i - 1] = '\b';
				memmove(string + i, string + i + 1, len - i);
				break;
			case 't':
				string[i - 1] = '\t';
				memmove(string + i, string + i + 1, len - i);
				break;
			case 'n':
				string[i - 1] = '\n';
				memmove(string + i, string + i + 1, len - i);
				break;
			case 'v':
				string[i - 1] = '\v';
				memmove(string + i, string + i + 1, len - i);
				break;
			case 'f':
				string[i - 1] = '\f';
				memmove(string + i, string + i + 1, len - i);
				break;
			case 'r':
				string[i - 1] = '\r';
				memmove(string + i, string + i + 1, len - i);
				break;
			}
		}
	}
	return len;
}
