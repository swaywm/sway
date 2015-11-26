#include "readline.h"
#include <stdlib.h>
#include <stdio.h>

char *read_line(FILE *file) {
	int length = 0, size = 128;
	char *string = malloc(size);
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
		if (length == size) {
			char *new_string = realloc(string, size *= 2);
			if (!new_string) {
				free(string);
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
