#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include "readline.h"
#include "stringop.h"
#include "list.h"
#include "log.h"
#include "commands.h"
#include "config.h"
#include "layout.h"

struct sway_config *config;

static bool exists(const char *path) {
	return access(path, R_OK) != -1;
}

void config_defaults(struct sway_config *config) {
	config->symbols = create_list();
	config->modes = create_list();
	config->cmd_queue = create_list();
	config->workspace_outputs = create_list();
	config->current_mode = malloc(sizeof(struct sway_mode));
	config->current_mode->name = NULL;
	config->current_mode->bindings = create_list();
	list_add(config->modes, config->current_mode);
	// Flags
	config->focus_follows_mouse = true;
	config->mouse_warping = true;
	config->reloading = false;
	config->active = false;
	config->failed = false;
	config->gaps_inner = 0;
	config->gaps_outer = 0;
}

void free_mode(struct sway_mode *mode) {
	free(mode->name);
	free_flat_list(mode->bindings);
}

void free_config(struct sway_config *config) {
	int i;
	for (i = 0; i < config->modes->length; ++i) {
		free_mode((struct sway_mode *)config->modes->items[i]);
	}
	free_flat_list(config->modes);
	for (i = 0; i < config->workspace_outputs->length; ++i) {
		struct workspace_output *wso = config->workspace_outputs->items[i];
		free(wso->output);
		free(wso->workspace);
	}
	free_flat_list(config->workspace_outputs);
	free_flat_list(config->cmd_queue);
	for (i = 0; i < config->symbols->length; ++i) {
		struct sway_variable *sym = config->symbols->items[i];
		free(sym->name);
		free(sym->value);
	}
	free_flat_list(config->symbols);
}

static const char *search_paths[] = {
	"$home/.sway/config",
	"$config/.sway/config",
	"/etc/sway/config",
	"$home/.i3/config",
	"$config/.i3/config",
	"/etc/i3/config"
};

static char *get_config_path() {
	char *home = getenv("HOME");
	char *config = getenv("XDG_CONFIG_HOME");
	if (!config) {
		const char *def = "/.config/sway";
		config = malloc(strlen(home) + strlen(def) + 1);
		strcpy(config, home);
		strcat(config, def);
	}

	// Set up a temporary config for holding set variables
	struct sway_config *temp_config = malloc(sizeof(struct sway_config));
	config_defaults(temp_config);
	const char *set_home = "set $home ";
	char *_home = malloc(strlen(home) + strlen(set_home) + 1);
	strcpy(_home, set_home);
	strcat(_home, home);
	handle_command(temp_config, _home);
	free(_home);
	const char *set_config = "set $config ";
	char *_config = malloc(strlen(config) + strlen(set_config) + 1);
	strcpy(_config, set_config);
	strcat(_config, config);
	handle_command(temp_config, _config);
	free(_config);

	char *test = NULL;
	int i;
	for (i = 0; i < sizeof(search_paths) / sizeof(char *); ++i) {
		test = strdup(search_paths[i]);
		test = do_var_replacement(temp_config, test);
		sway_log(L_DEBUG, "Checking for config at %s", test);
		if (exists(test)) {
			goto _continue;
		}
		free(test);
		test = NULL;
	}

	sway_log(L_DEBUG, "Trying to find config in XDG_CONFIG_DIRS");
	char *xdg_config_dirs = getenv("XDG_CONFIG_DIRS");
	if (xdg_config_dirs != NULL) {
		list_t *paths = split_string(xdg_config_dirs, ":");
		char *name = "/sway/config";
		int i;
		for (i = 0; i < paths->length; i++ ) {
			test = malloc(strlen(paths->items[i]) + strlen(name) + 1);
			strcpy(test, paths->items[i]);
			strcat(test, name);
			if (exists(test)) {
				free_flat_list(paths);
				return test;
			}
			free(test);
		}
		free_flat_list(paths);
	}

_continue:
	free_config(temp_config);
	return test;
}

bool load_config(void) {
	sway_log(L_INFO, "Loading config");

	char *path = get_config_path();

	if (path == NULL) {
		sway_log(L_ERROR, "Unable to find a config file!");
		return false;
	}

	FILE *f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "Unable to open %s for reading", path);
		free(path);
		return false;
	}
	free(path);

	bool config_load_success;
	if (config) {
		config_load_success = read_config(f, true);
	} else {
		config_load_success = read_config(f, false);
	}
	fclose(f);

	return config_load_success;
}

bool read_config(FILE *file, bool is_active) {
	struct sway_config *temp_config = malloc(sizeof(struct sway_config));
	config_defaults(temp_config);
	if (is_active) {
		sway_log(L_DEBUG, "Performing configuration file reload");
		temp_config->reloading = true;
		temp_config->active = true;
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

		// Any command which would require wlc to be initialized
		// should be queued for later execution
		list_t *args = split_string(line, " ");
		if (!is_active && (
			strcmp("exec", args->items[0]) == 0 ||
			strcmp("exec_always", args->items[0]) == 0 )) {
			sway_log(L_DEBUG, "Deferring command %s", line);

			char *cmd = malloc(strlen(line) + 1);
			strcpy(cmd, line);
			list_add(temp_config->cmd_queue, cmd);
		} else if (!temp_depth && !handle_command(temp_config, line)) {
			sway_log(L_DEBUG, "Config load failed for line %s", line);
			success = false;
			temp_config->failed = true;
		}
		list_free(args);

_continue:
		if (line && line[strlen(line) - 1] == '{') {
			temp_depth++;
		}
		free(line);
	}

	if (is_active) {
		temp_config->reloading = false;
		container_map(&root_container, reset_gaps, NULL);
		arrange_windows(&root_container, -1, -1);
	}
	config = temp_config;

	return success;
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
