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
			string = realloc(string, size *= 2);
			if (!string) {
				return NULL;
			}
		}
		string[length++] = c;
	}
	if (length + 1 == size) {
		string = realloc(string, length + 1);
		if (!string) {
			return NULL;
		}
	}
	string[length] = '\0';
	return string;
}
