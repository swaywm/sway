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

struct sway_config *config;

static bool exists(const char *path) {
	return access(path, R_OK) != -1;
}

static char* get_config_path() {
	char *name = "/.sway/config";
	const char *home = getenv("HOME");

	// Check home dir
	sway_log(L_DEBUG, "Trying to find config in ~/.sway/config");
	char *temp = malloc(strlen(home) + strlen(name) + 1);
	strcpy(temp, home);
	strcat(temp, name);
	if (exists(temp)) {
		return temp;
	}

	// Check XDG_CONFIG_HOME with fallback to ~/.config/
	sway_log(L_DEBUG, "Trying to find config in XDG_CONFIG_HOME/sway/config");
	char *xdg_config_home = getenv("XDG_CONFIG_HOME");
	if (xdg_config_home == NULL) {
		sway_log(L_DEBUG, "Falling back to ~/.config/sway/config");
		name = "/.config/sway/config";
		temp = malloc(strlen(home) + strlen(name) + 1);
		strcpy(temp, home);
		strcat(temp, name);
	} else {
		name = "/sway/config";
		temp = malloc(strlen(xdg_config_home) + strlen(name) + 1);
		strcpy(xdg_config_home, home);
		strcat(temp, name);
	}
	if (exists(temp)) {
		return temp;
	}

	// Check /etc/
	sway_log(L_DEBUG, "Trying to find config in /etc/sway/config");
	strcpy(temp, "/etc/sway/config");
	if (exists(temp)) {
		return temp;
	}

	// Check XDG_CONFIG_DIRS
	sway_log(L_DEBUG, "Trying to find config in XDG_CONFIG_DIRS");
	char *xdg_config_dirs = getenv("XDG_CONFIG_DIRS");
	if (xdg_config_dirs != NULL) {
		list_t *paths = split_string(xdg_config_dirs, ":");
		name = "/sway/config";
		int i;
		for (i = 0; i < paths->length; i++ ) {
			temp = malloc(strlen(paths->items[i]) + strlen(name) + 1);
			strcpy(temp, paths->items[i]);
			strcat(temp, name);
			if (exists(temp)) {
				free_flat_list(paths);
				return temp;
			}
		}
		free_flat_list(paths);
	}

	//Now fall back to i3 paths and try the same thing
	name = "/.i3/config";
	sway_log(L_DEBUG, "Trying to find config in ~/.i3/config");
	temp = malloc(strlen(home) + strlen(name) + 1);
	strcpy(temp, home);
	strcat(temp, name);
	if (exists(temp)) {
		return temp;
	}

	sway_log(L_DEBUG, "Trying to find config in XDG_CONFIG_HOME/i3/config");
	if (xdg_config_home == NULL) {
		sway_log(L_DEBUG, "Falling back to ~/.config/i3/config");
		name = "/.config/i3/config";
		temp = malloc(strlen(home) + strlen(name) + 1);
		strcpy(temp, home);
		strcat(temp, name);
	} else {
		name = "/i3/config";
		temp = malloc(strlen(xdg_config_home) + strlen(name) + 1);
		strcpy(xdg_config_home, home);
		strcat(temp, name);
	}
	if (exists(temp)) {
		return temp;
	}

	sway_log(L_DEBUG, "Trying to find config in /etc/i3/config");
	strcpy(temp, "/etc/i3/config");
	if (exists(temp)) {
		return temp;
	}

	sway_log(L_DEBUG, "Trying to find config in XDG_CONFIG_DIRS");
	if (xdg_config_dirs != NULL) {
		list_t *paths = split_string(xdg_config_dirs, ":");
		name = "/i3/config";
		int i;
		for (i = 0; i < paths->length; i++ ) {
			temp = malloc(strlen(paths->items[i]) + strlen(name) + 1);
			strcpy(temp, paths->items[i]);
			strcat(temp, name);
			if (exists(temp)) {
				free_flat_list(paths);
				return temp;
			}
		}
		free_flat_list(paths);
	}

	return NULL;
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
		char *line = read_line(file);
		strip_comments(line);
		strip_whitespace(line);
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
