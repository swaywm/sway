#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "readline.h"
#include "stringop.h"
#include "list.h"
#include "commands.h"
#include "config.h"

struct sway_config *read_config(FILE *file) {
	struct sway_config *config = malloc(sizeof(struct sway_config));
	config->symbols = create_list();
	config->modes = create_list();

	int temp_braces = 0; // Temporary: skip all config sections with braces

	while (!feof(file)) {
		int _;
		char *line = read_line(file);
		line = strip_whitespace(line, &_);
		line = strip_comments(line);
		if (!line[0]) {
			goto _continue;
		}
		if (temp_braces && line[0] == '}') {
			temp_braces--;
			goto _continue;
		}

		handle_command(config, line);
		
_continue:
		if (line && line[strlen(line) - 1] == '{') {
			temp_braces++;
		}
		free(line);
	}

	return config;
}
