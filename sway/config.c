#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "readline.h"
#include "stringop.h"
#include "list.h"
#include "log.h"
#include "commands.h"
#include "config.h"

void config_defaults(struct sway_config *config) {
	config->symbols = create_list();
	config->modes = create_list();
	config->current_mode = malloc(sizeof(struct sway_mode));
	config->current_mode->name = NULL;
	config->current_mode->bindings = create_list();
	list_add(config->modes, config->current_mode);
	// Flags
	config->focus_follows_mouse = true;
	config->mouse_warping = true;
    config->reloading = false; 
}

struct sway_config *read_config(FILE *file, bool is_active) {
	struct sway_config *config = malloc(sizeof(struct sway_config));
	config_defaults(config);

    if (is_active) {
        config->reloading = true; 
    }

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

		if (!temp_depth && handle_command(config, line) != 0) {
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

    config->reloading = false; 

	return config;
}

char *do_var_replacement(struct sway_config *config, char *str) {
	// TODO: Handle escaping $ and using $ in string literals
	int i;
	for (i = 0; str[i]; ++i) {
		if (str[i] == '$') {
			// Try for match (note: this could be faster)
			int j;
			for (j = 0; j < config->symbols->length; ++j) {
				struct sway_variable *var = config->symbols->items[j];
				if (strstr(str + i, var->name) == str + i) {
					// Match, do replacement
					char *new_string = malloc(
						strlen(str) -
						strlen(var->name) +
						strlen(var->value) + 1);
					strncpy(new_string, str, i);
					new_string[i] = 0;
					strcat(new_string, var->value);
					strcat(new_string, str + i + strlen(var->name));
					free(str);
					str = new_string;
				}
			}
		}
	}
	return str;
}
