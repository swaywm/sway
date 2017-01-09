#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "readline.h"
#include "stringop.h"

/***************************************************************************//**
*
* \brief			Read one line of the configuration file.
*
* \param			file	Pointer to the already open configuration file. If NULL is
*										passed, no operation is performed.
*
* \return			String containing one single line from the configuration file.
*							This string is not necessarily in the format how it is expected
*							for further processing of configuration commands. NULL is
*							returned in case of failure or if NULL is passed for 'file'.
*
******************************************************************************/
inline char* read_line(FILE* restrict const file) {
	char* line = NULL;
	if (file) {
		size_t line_length = 0u;
		const ssize_t bytes_read = getline(&line, &line_length, file);
		if (line) {
			assert(line_length);
			if (0 > bytes_read) {
				free(line);
				line = NULL;
			} else {
				strip_whitespace(line);
				if (line[0] == '#') {
					free(line);
					line = NULL;
				}
			}
		}
	}
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
