#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "stringop.h"
#include "log.h"
#include "string.h"
#include "list.h"

const char whitespace[] = " \f\n\r\t\v";

/**************************************************************************//**
 *
 * \brief		Strips the whitespace in front and rear of the string. If whitespace
 *					is found in the middle of the string, all but one space is removed
 *					between words.
 *
 * \param		str	Pointer to the char string that should be stripped. The string
 *							is modified. If NULL is passed, no operation is performed.
 *
 ******************************************************************************/
void strip_whitespace(char * restrict const str) {
	if (str) {
		size_t count = 0;
		for (size_t index = 0; str[index]; ++index) {
			if (' ' != str[index] && '\t' != str[index]) {
				str[count++] = str[index];
			} else if(0 < count && ' ' != str[count - 1] && '\t' != str[count - 1]) {
				str[count++] = ' ';
			}
		}
		str[count] = '\0';
		if (0 < count && (' ' == str[count - 1] || '\t' == str[count - 1])) {
			str[count - 1] = '\0';
		}
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

// strcmp that also handles null pointers.
int lenient_strcmp(char *a, char *b) {
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
	char *token;

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
	char **argv = malloc(sizeof(char *) * alloc);
	bool in_token = false;
	bool in_string = false;
	bool in_char = false;
	bool in_brackets = false; // brackets are used for critera
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

static bool has_whitespace(const char *str) {
	while (*str) {
		if (isspace(*str)) {
			return true;
		}
		++str;
	}
	return false;
}


/*
 * Returns true if the string contains only whitespace and false otherwise.
 */
bool is_empty(const char *str) {
	if(!str) {
		return true;
	}
	while (*str != '\0') {
		if (!isspace(*(unsigned char*)str)) {
			return false;
		}
		str++;
	}
	return true;
}

/**
 * Add quotes around any argv with whitespaces.
 */
void add_quotes(char **argv, int argc) {
	int i;
	for (i = 0; i < argc; ++i) {
		if (has_whitespace(argv[i])) {
			int len = strlen(argv[i]) + 3;
			char *tmp = argv[i];
			argv[i] = malloc(len * sizeof(char));
			snprintf(argv[i], len, "\"%s\"", tmp);
			free(tmp);
		}
	}
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

char *cmdsep(char **stringp, const char *delim) {
	// skip over leading delims
	char *head = *stringp + strspn(*stringp, delim);
	// Find end token
	char *tail = *stringp += strcspn(*stringp, delim);
	// Set stringp to beginning of next token
	*stringp += strspn(*stringp, delim);
	// Set stringp to null if last token
	if (!**stringp) *stringp = NULL;
	// Nullify end of first token
	*tail = 0;
	return head;
}

char *argsep(char **stringp, const char *delim) {
	char *start = *stringp;
	char *end = start;
	bool in_string = false;
	bool in_char = false;
	bool escaped = false;
	while (1) {
		if (*end == '"' && !in_char && !escaped) {
			in_string = !in_string;
		} else if (*end == '\'' && !in_string && !escaped) {
			in_char = !in_char;
		} else if (*end == '\\') {
			escaped = !escaped;
		} else if (*end == '\0') {
			*stringp = NULL;
			goto found;
		} else if (!in_string && !in_char && !escaped && strchr(delim, *end)) {
			if (end - start) {
				*(end++) = 0;
				*stringp = end + strspn(end, delim);;
				if (!**stringp) *stringp = NULL;
				goto found;
			} else {
				++start;
				end = start;
			}
		}
		if (*end != '\\') {
			escaped = false;
		}
		++end;
	}
	found:
	return start;
}
