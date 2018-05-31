#define _POSIX_C_SOURCE 200809L
#include "readline.h"
#include "log.h"
#include <stdlib.h>
#include <stdio.h>

char *read_line(FILE *file) {
	size_t length = 0, size = 128;
	char *string = malloc(size);
	char lastChar = '\0';
	if (!string) {
		wlr_log(L_ERROR, "Unable to allocate memory for read_line");
		return NULL;
	}
	while (1) {
		int c = getc(file);
		if (c == '\n' && lastChar == '\\'){
			--length; // Ignore last character.
			lastChar = '\0';
			continue;
		}
		if (c == EOF || c == '\n' || c == '\0') {
			break;
		}
		if (c == '\r') {
			continue;
		}
		lastChar = c;
		if (length == size) {
			char *new_string = realloc(string, size *= 2);
			if (!new_string) {
				free(string);
				wlr_log(L_ERROR, "Unable to allocate memory for read_line");
				return NULL;
			}
			string = new_string;
		}
		string[length++] = c;
	}
	if (length + 1 == size) {
		char *new_string = realloc(string, length + 1);
		if (!new_string) {
			free(string);
			return NULL;
		}
		string = new_string;
	}
	string[length] = '\0';
	return string;
}

char *peek_line(FILE *file, int offset, long *position) {
	long pos = ftell(file);
	size_t length = 1;
	char *line = calloc(1, length);
	for (int i = 0; i <= offset; i++) {
		ssize_t read = getline(&line, &length, file);
		if (read < 0) {
			break;
		}
		if (read > 0 && line[read - 1] == '\n') {
			line[read - 1] = '\0';
		}
	}
	if (position) {
		*position = ftell(file);
	}
	fseek(file, pos, SEEK_SET);
	return line;
}

char *read_line_buffer(FILE *file, char *string, size_t string_len) {
	size_t length = 0;
	if (!string) {
		return NULL;
	}
	while (1) {
		int c = getc(file);
		if (c == EOF || c == '\n' || c == '\0') {
			break;
		}
		if (c == '\r') {
			continue;
		}
		string[length++] = c;
		if (string_len <= length) {
			return NULL;
		}
	}
	if (length + 1 == string_len) {
		return NULL;
	}
	string[length] = '\0';
	return string;
}
