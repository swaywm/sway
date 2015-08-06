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
	config->current_mode = malloc(sizeof(struct sway_mode));
	config->current_mode->name = NULL;
	config->current_mode->bindings = create_list();
	list_add(config->modes, config->current_mode);

	bool success = true;

	int temp_depth = 0; // Temporary: skip all config sections with depth

	while (!feof(file)) {
		int _;
		char *line = read_line(file);
		line = strip_whitespace(line, &_);
		line = strip_comments(line);
		if (!line[0]) {
			goto _continue;
		}
		if (temp_depth && line[0] == '}') {
			temp_depth--;
			goto _continue;
		}

		if (!handle_command(config, line)) {
			success = false;
		}
		
_continue:
		if (line && line[strlen(line) - 1] == '{') {
			temp_depth++;
		}
		free(line);
	}

	if (!success) {
		exit(1);
	}

	return config;
}
