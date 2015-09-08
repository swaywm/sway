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
#include "input_state.h"

struct sway_config *config = NULL;


static void free_variable(struct sway_variable *var) {
	free(var->name);
	free(var->value);
	free(var);
}

static void free_binding(struct sway_binding *bind) {
	free_flat_list(bind->keys);
	free(bind->command);
	free(bind);
}

static void free_mode(struct sway_mode *mode) {
	free(mode->name);
	int i;
	for (i = 0; i < mode->bindings->length; ++i) {
		free_binding(mode->bindings->items[i]);
	}
	list_free(mode->bindings);
	free(mode);
}

static void free_output_config(struct output_config *oc) {
	free(oc->name);
	free(oc);
}

static void free_workspace_output(struct workspace_output *wo) {
	free(wo->output);
	free(wo->workspace);
	free(wo);
}

static void free_config(struct sway_config *config) {
	int i;
	for (i = 0; i < config->symbols->length; ++i) {
		free_variable(config->symbols->items[i]);
	}
	list_free(config->symbols);

	for (i = 0; i < config->modes->length; ++i) {
		free_mode(config->modes->items[i]);
	}
	list_free(config->modes);

	free_flat_list(config->cmd_queue);

	for (i = 0; i < config->workspace_outputs->length; ++i) {
		free_workspace_output(config->workspace_outputs->items[i]);
	}
	list_free(config->workspace_outputs);

	for (i = 0; i < config->output_configs->length; ++i) {
		free_output_config(config->output_configs->items[i]);
	}
	list_free(config->output_configs);
	free(config);
}


static bool file_exists(const char *path) {
	return access(path, R_OK) != -1;
}

static void config_defaults(struct sway_config *config) {
	config->symbols = create_list();
	config->modes = create_list();
	config->workspace_outputs = create_list();
	config->output_configs = create_list();

	config->cmd_queue = create_list();

	config->current_mode = malloc(sizeof(struct sway_mode));
	config->current_mode->name = malloc(sizeof("default"));
	strcpy(config->current_mode->name, "default");
	config->current_mode->bindings = create_list();
	list_add(config->modes, config->current_mode);

	config->floating_mod = 0;
	config->default_layout = L_NONE;
	config->default_orientation = L_NONE;
	// Flags
	config->focus_follows_mouse = true;
	config->mouse_warping = true;
	config->reloading = false;
	config->active = false;
	config->failed = false;
	config->auto_back_and_forth = false;

	config->gaps_inner = 0;
	config->gaps_outer = 0;
}

static char *get_config_path(void) {
	char *config_path = NULL;
	char *paths[3] = {getenv("HOME"), getenv("XDG_CONFIG_HOME"), ""};
	int pathlen[3] = {0, 0, 0};
	int i;
#define home paths[0]
#define conf paths[1]
	// Get home and config directories
	home = home ? strdup(home) : NULL;
	if (conf) {
		conf = strdup(conf);
	} else if (home) {
		const char *def = "/.config";
		conf = malloc(strlen(home) + strlen(def) + 1);
		strcpy(conf, home);
		strcat(conf, def);
	} else {
		home = strdup("");
		conf = strdup("");
	}
	pathlen[0] = strlen(home);
	pathlen[1] = strlen(conf);
#undef home
#undef conf
	// Search for config file from search paths
	static const char *search_paths[] = {
		"/.sway/config", // Prepend with $home
		"/sway/config", // Prepend with $config
		"/etc/sway/config",
		"/.i3/config", // $home
		"/.i3/config", // $config
		"/etc/i3/config"
	};
	for (i = 0; i < (int)(sizeof(search_paths) / sizeof(char *)); ++i) {
		char *test = malloc(pathlen[i%3] + strlen(search_paths[i]) + 1);
		strcpy(test, paths[i%3]);
		strcat(test, search_paths[i]);
		sway_log(L_DEBUG, "Checking for config at %s", test);
		if (file_exists(test)) {
			config_path = test;
			goto cleanup;
		}
		free(test);
	}

	sway_log(L_DEBUG, "Trying to find config in XDG_CONFIG_DIRS");
	char *xdg_config_dirs = getenv("XDG_CONFIG_DIRS");
	if (xdg_config_dirs) {
		list_t *paths = split_string(xdg_config_dirs, ":");
		const char *name = "/sway/config";
		for (i = 0; i < paths->length; i++ ) {
			char *test = malloc(strlen(paths->items[i]) + strlen(name) + 1);
			strcpy(test, paths->items[i]);
			strcat(test, name);
			if (file_exists(test)) {
				config_path = test;
				break;
			}
			free(test);
		}
		free_flat_list(paths);
	}

cleanup:
	free(paths[0]);
	free(paths[1]);
	return config_path;
}

bool load_config(const char *file) {
	sway_log(L_INFO, "Loading config");

	input_init();

	char *path;
	if (file != NULL) {
		path = strdup(file);
	} else {
		path = get_config_path();
	}

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
	struct sway_config *old_config = config;
	config = malloc(sizeof(struct sway_config));

	config_defaults(config);
	if (is_active) {
		sway_log(L_DEBUG, "Performing configuration file reload");
		config->reloading = true;
		config->active = true;
	}
	bool success = true;

	char *line;
	while (!feof(file)) {
		line = read_line(file);
		line = strip_comments(line);
		if (!config_command(line)) {
			success = false;
		}
		free(line);
	}

	if (is_active) {
		config->reloading = false;
		arrange_windows(&root_container, -1, -1);
	}
	if (old_config) {
		free_config(old_config);
	}

	return success;
}

char *do_var_replacement(char *str) {
	int i;
	char *find = str;
	while ((find = strchr(find, '$'))) {
		// Skip if escaped.
		if (find > str && find[-1] == '\\') {
			if (find == str + 1 || !(find > str + 1 && find[-2] == '\\')) {
				++find;
				continue;
			}
		}
		// Find matching variable
		for (i = 0; i < config->symbols->length; ++i) {
			struct sway_variable *var = config->symbols->items[i];
			int vnlen = strlen(var->name);
			if (strncmp(find, var->name, vnlen) == 0) {
				int vvlen = strlen(var->value);
				char *newstr = malloc(strlen(str) - vnlen + vvlen + 1);
				char *newptr = newstr;
				int offset = find - str;
				strncpy(newptr, str, offset);
				newptr += offset;
				strncpy(newptr, var->value, vvlen);
				newptr += vvlen;
				strcpy(newptr, find + vnlen);
				free(str);
				str = newstr;
				find = str + offset + vvlen;
				break;
			}
		}
		if (i == config->symbols->length) {
			++find;
		}
	}
	return str;
}
