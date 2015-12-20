#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <wordexp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "wayland-desktop-shell-server-protocol.h"
#include "readline.h"
#include "stringop.h"
#include "list.h"
#include "log.h"
#include "commands.h"
#include "config.h"
#include "layout.h"
#include "input_state.h"
#include "criteria.h"

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

static void free_bar(struct bar_config *bar) {
	free(bar->mode);
	free(bar->hidden_state);
	free(bar->status_command);
	free(bar->font);
	free(bar->separator_symbol);
	int i;
	for (i = 0; i < bar->bindings->length; ++i) {
		free_sway_mouse_binding(bar->bindings->items[i]);
	}
	list_free(bar->bindings);

	free_flat_list(bar->outputs);
	free(bar);
}

void free_output_config(struct output_config *oc) {
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

	for (i = 0; i < config->bars->length; ++i) {
		free_bar(config->bars->items[i]);
	}
	list_free(config->bars);

	free_flat_list(config->cmd_queue);

	for (i = 0; i < config->workspace_outputs->length; ++i) {
		free_workspace_output(config->workspace_outputs->items[i]);
	}
	list_free(config->workspace_outputs);

	for (i = 0; i < config->criteria->length; ++i) {
		free_criteria(config->criteria->items[i]);
	}
	list_free(config->criteria);

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
	config->bars = create_list();
	config->workspace_outputs = create_list();
	config->criteria = create_list();
	config->output_configs = create_list();

	config->cmd_queue = create_list();

	config->current_mode = malloc(sizeof(struct sway_mode));
	config->current_mode->name = malloc(sizeof("default"));
	strcpy(config->current_mode->name, "default");
	config->current_mode->bindings = create_list();
	list_add(config->modes, config->current_mode);

	config->floating_mod = 0;
	config->dragging_key = M_LEFT_CLICK;
	config->resizing_key = M_RIGHT_CLICK;
	config->default_layout = L_NONE;
	config->default_orientation = L_NONE;
	// Flags
	config->focus_follows_mouse = true;
	config->mouse_warping = true;
	config->reloading = false;
	config->active = false;
	config->failed = false;
	config->auto_back_and_forth = false;
	config->seamless_mouse = true;
	config->reading = false;

	config->edge_gaps = true;
	config->gaps_inner = 0;
	config->gaps_outer = 0;
}

static char *get_config_path(void) {
	static const char *config_paths[] = {
		"$HOME/.sway/config",
		"$XDG_CONFIG_HOME/sway/config",
		"$HOME/.i3/config",
		"$XDG_CONFIG_HOME/i3/config",
		FALLBACK_CONFIG_DIR "/config",
		"/etc/i3/config",
	};

	if (!getenv("XDG_CONFIG_HOME")) {
		char *home = getenv("HOME");
		char *config_home = malloc(strlen(home) + strlen("/.config") + 1);
		strcpy(config_home, home);
		strcat(config_home, "/.config");
		setenv("XDG_CONFIG_HOME", config_home, 1);
		sway_log(L_DEBUG, "Set XDG_CONFIG_HOME to %s", config_home);
	}

	wordexp_t p;
	char *path;

	int i;
	for (i = 0; i < (int)(sizeof(config_paths) / sizeof(char *)); ++i) {
		if (wordexp(config_paths[i], &p, 0) == 0) {
			path = p.we_wordv[0];
			if (file_exists(path)) {
				return path;
			}
		}
	}

	return NULL; // Not reached
}

bool load_config(const char *file) {
	input_init();

	char *path;
	if (file != NULL) {
		path = strdup(file);
	} else {
		path = get_config_path();
	}

	sway_log(L_INFO, "Loading config from %s", path);

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
	config = calloc(1, sizeof(struct sway_config));

	config_defaults(config);
	config->reading = true;
	if (is_active) {
		sway_log(L_DEBUG, "Performing configuration file reload");
		config->reloading = true;
		config->active = true;
	}
	bool success = true;
	enum cmd_status block = CMD_BLOCK_END;

	int line_number = 0;
	char *line;
	while (!feof(file)) {
		line = read_line(file);
		line_number++;
		line = strip_whitespace(line);
		if (line[0] == '#') {
			continue;
		}
		struct cmd_results *res = config_command(line, block);
		switch(res->status) {
		case CMD_FAILURE:
		case CMD_INVALID:
			sway_log(L_ERROR, "Error on line %i '%s': %s", line_number, line,
				res->error);
			success = false;
			break;

		case CMD_DEFER:
			sway_log(L_DEBUG, "Defferring command `%s'", line);
			list_add(config->cmd_queue, strdup(line));
			break;

		case CMD_BLOCK_MODE:
			if (block == CMD_BLOCK_END) {
				block = CMD_BLOCK_MODE;
			} else {
				sway_log(L_ERROR, "Invalid block '%s'", line);
			}
			break;

		case CMD_BLOCK_BAR:
			if (block == CMD_BLOCK_END) {
				block = CMD_BLOCK_BAR;
			} else {
				sway_log(L_ERROR, "Invalid block '%s'", line);
			}
			break;

		case CMD_BLOCK_BAR_COLORS:
			if (block == CMD_BLOCK_BAR) {
				block = CMD_BLOCK_BAR_COLORS;
			} else {
				sway_log(L_ERROR, "Invalid block '%s'", line);
			}
			break;

		case CMD_BLOCK_END:
			switch(block) {
			case CMD_BLOCK_MODE:
				sway_log(L_DEBUG, "End of mode block");
				config->current_mode = config->modes->items[0];
				block = CMD_BLOCK_END;
				break;

			case CMD_BLOCK_BAR:
				sway_log(L_DEBUG, "End of bar block");
				config->current_bar = NULL;
				block = CMD_BLOCK_END;
				break;

			case CMD_BLOCK_BAR_COLORS:
				sway_log(L_DEBUG, "End of bar colors block");
				block = CMD_BLOCK_BAR;
				break;

			case CMD_BLOCK_END:
				sway_log(L_ERROR, "Unmatched }");
				break;

			default:;
			}
		default:;
		}
		free(line);
		free(res);
	}

	if (is_active) {
		config->reloading = false;
		arrange_windows(&root_container, -1, -1);
	}
	if (old_config) {
		free_config(old_config);
	}

	config->reading = false;
	return success;
}

int output_name_cmp(const void *item, const void *data) {
	const struct output_config *output = item;
	const char *name = data;

	return strcmp(output->name, name);
}

void merge_output_config(struct output_config *dst, struct output_config *src) {
	if (src->name) {
		if (dst->name) {
			free(dst->name);
		}
		dst->name = strdup(src->name);
	}
	if (src->enabled != -1) {
		dst->enabled = src->enabled;
	}
	if (src->width != -1) {
		dst->width = src->width;
	}
	if (src->height != -1) {
		dst->height = src->height;
	}
	if (src->x != -1) {
		dst->x = src->x;
	}
	if (src->y != -1) {
		dst->y = src->y;
	}
	if (src->background) {
		if (dst->background) {
			free(dst->background);
		}
		dst->background = strdup(src->background);
	}
	if (src->background_option) {
		if (dst->background_option) {
			free(dst->background_option);
		}
		dst->background_option = strdup(src->background_option);
	}
}

static void invoke_swaybar(swayc_t *output, struct bar_config *bar, int output_i) {
	sway_log(L_DEBUG, "Invoking swaybar for output %s[%d] and bar %s", output->name, output_i, bar->id);

	size_t bufsize = 4;
	char output_id[bufsize];
	snprintf(output_id, bufsize, "%d", output_i);
	output_id[bufsize-1] = 0;

	pid_t *pid = malloc(sizeof(pid_t));
	*pid = fork();
	if (*pid == 0) {
		if (!bar->swaybar_command) {
			char *const cmd[] = {
				"swaybar",
				"-b",
				bar->id,
				output_id,
				NULL,
			};

			execvp(cmd[0], cmd);
		} else {
			// run custom swaybar
			int len = strlen(bar->swaybar_command) + strlen(bar->id) + strlen(output_id) + 6;
			char *command = malloc(len * sizeof(char));
			snprintf(command, len, "%s -b %s %s", bar->swaybar_command, bar->id, output_id);

			char *const cmd[] = {
				"sh",
				"-c",
				command,
				NULL,
			};

			execvp(cmd[0], cmd);
			free(command);
		}
	}

	// add swaybar pid to output
	list_add(output->bar_pids, pid);
}

void terminate_swaybars(list_t *pids) {
	int i, ret;
	pid_t *pid;
	for (i = 0; i < pids->length; ++i) {
		pid = pids->items[i];
		ret = kill(*pid, SIGTERM);
		if (ret != 0) {
			sway_log(L_ERROR, "Unable to terminate swaybar [pid: %d]", *pid);
		} else {
			int status;
			waitpid(*pid, &status, 0);
		}
	}

	// empty pids list
	for (i = 0; i < pids->length; ++i) {
		pid = pids->items[i];
		list_del(pids, i);
		free(pid);
	}
}

void terminate_swaybg(pid_t pid) {
	int ret = kill(pid, SIGTERM);
	if (ret != 0) {
		sway_log(L_ERROR, "Unable to terminate swaybg [pid: %d]", pid);
	} else {
		int status;
		waitpid(pid, &status, 0);
	}
}

void load_swaybars(swayc_t *output, int output_idx) {
	// Check for bars
	list_t *bars = create_list();
	struct bar_config *bar = NULL;
	int i;
	for (i = 0; i < config->bars->length; ++i) {
		bar = config->bars->items[i];
		bool apply = false;
		if (bar->outputs) {
			int j;
			for (j = 0; j < bar->outputs->length; ++j) {
				char *o = bar->outputs->items[j];
				if (!strcmp(o, "*") || !strcasecmp(o, output->name)) {
					apply = true;
					break;
				}
			}
		} else {
			apply = true;
		}
		if (apply) {
			list_add(bars, bar);
		}
	}

	// terminate swaybar processes previously spawned for this
	// output.
	terminate_swaybars(output->bar_pids);

	for (i = 0; i < bars->length; ++i) {
		bar = bars->items[i];
		invoke_swaybar(output, bar, output_idx);
	}

	list_free(bars);
}

void apply_output_config(struct output_config *oc, swayc_t *output) {
	if (oc && oc->width > 0 && oc->height > 0) {
		output->width = oc->width;
		output->height = oc->height;

		sway_log(L_DEBUG, "Set %s size to %ix%i", oc->name, oc->width, oc->height);
		struct wlc_size new_size = { .w = oc->width, .h = oc->height };
		wlc_output_set_resolution(output->handle, &new_size);
	}

	// Find position for it
	if (oc && oc->x != -1 && oc->y != -1) {
		sway_log(L_DEBUG, "Set %s position to %d, %d", oc->name, oc->x, oc->y);
		output->x = oc->x;
		output->y = oc->y;
	} else {
		int x = 0;
		for (int i = 0; i < root_container.children->length; ++i) {
			swayc_t *c = root_container.children->items[i];
			if (c->type == C_OUTPUT) {
				if (c->width + c->x > x) {
					x = c->width + c->x;
				}
			}
		}
		output->x = x;
	}

	if (!oc || !oc->background) {
		// Look for a * config for background
		int i = list_seq_find(config->output_configs, output_name_cmp, "*");
		if (i >= 0) {
			oc = config->output_configs->items[i];
		} else {
			oc = NULL;
		}
	}

	int output_i;
	for (output_i = 0; output_i < root_container.children->length; ++output_i) {
		if (root_container.children->items[output_i] == output) {
			break;
		}
	}

	if (oc && oc->background) {

		if (output->bg_pid != 0) {
			terminate_swaybg(output->bg_pid);
		}

		sway_log(L_DEBUG, "Setting background for output %d to %s", output_i, oc->background);

		size_t bufsize = 4;
		char output_id[bufsize];
		snprintf(output_id, bufsize, "%d", output_i);
		output_id[bufsize-1] = 0;

		char *const cmd[] = {
			"swaybg",
			output_id,
			oc->background,
			oc->background_option,
			NULL,
		};

		output->bg_pid = fork();
		if (output->bg_pid == 0) {
			execvp(cmd[0], cmd);
		}
	}

	// load swaybars for output
	load_swaybars(output, output_i);
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

// the naming is intentional (albeit long): a workspace_output_cmp function
// would compare two structs in full, while this method only compares the
// workspace.
int workspace_output_cmp_workspace(const void *a, const void *b) {
	const struct workspace_output *wsa = a, *wsb = b;
	return lenient_strcmp(wsa->workspace, wsb->workspace);
}

int sway_binding_cmp_keys(const void *a, const void *b) {
	const struct sway_binding *binda = a, *bindb = b;

	// Count keys pressed for this binding. important so we check long before
	// short ones.  for example mod+a+b  before  mod+a
	unsigned int moda = 0, modb = 0, i;

	// Count how any modifiers are pressed
	for (i = 0; i < 8 * sizeof(binda->modifiers); ++i) {
		moda += (binda->modifiers & 1 << i) != 0;
		modb += (bindb->modifiers & 1 << i) != 0;
	}
	if (bindb->keys->length + modb != binda->keys->length + moda) {
		return (bindb->keys->length + modb) - (binda->keys->length + moda);
	}

	// Otherwise compare keys
	if (binda->modifiers > bindb->modifiers) {
		return 1;
	} else if (binda->modifiers < bindb->modifiers) {
		return -1;
	}
	for (int i = 0; i < binda->keys->length; i++) {
		xkb_keysym_t *ka = binda->keys->items[i],
				*kb = bindb->keys->items[i];
		if (*ka > *kb) {
			return 1;
		} else if (*ka < *kb) {
			return -1;
		}
	}
	return 0;
}

int sway_binding_cmp(const void *a, const void *b) {
	int cmp = 0;
	if ((cmp = sway_binding_cmp_keys(a, b)) != 0) {
		return cmp;
	}
	const struct sway_binding *binda = a, *bindb = b;
	return lenient_strcmp(binda->command, bindb->command);
}

void free_sway_binding(struct sway_binding *binding) {
	if (binding->keys) {
		for (int i = 0; i < binding->keys->length; i++) {
			free(binding->keys->items[i]);
		}
		list_free(binding->keys);
	}
	if (binding->command) {
		free(binding->command);
	}
	free(binding);
}

int sway_mouse_binding_cmp_buttons(const void *a, const void *b) {
	const struct sway_mouse_binding *binda = a, *bindb = b;
	if (binda->button > bindb->button) {
		return 1;
	}
	if (binda->button < bindb->button) {
		return -1;
	}
	return 0;
}

int sway_mouse_binding_cmp(const void *a, const void *b) {
	int cmp = 0;
	if ((cmp = sway_binding_cmp_keys(a, b)) != 0) {
		return cmp;
	}
	const struct sway_mouse_binding *binda = a, *bindb = b;
	return lenient_strcmp(binda->command, bindb->command);
}

void free_sway_mouse_binding(struct sway_mouse_binding *binding) {
	if (binding->command) {
		free(binding->command);
	}
	free(binding);
}

struct bar_config *default_bar_config(void) {
	struct bar_config *bar = NULL;
	bar = malloc(sizeof(struct bar_config));
	bar->mode = strdup("dock");
	bar->hidden_state = strdup("hide");
	bar->modifier = 0;
	bar->outputs = NULL;
	bar->position = DESKTOP_SHELL_PANEL_POSITION_BOTTOM;
	bar->bindings = create_list();
	bar->status_command = strdup("while :; do date +'%Y-%m-%d %l:%M:%S %p' && sleep 1; done");
	bar->swaybar_command = NULL;
	bar->font = strdup("monospace 10");
	bar->height = -1;
	bar->workspace_buttons = true;
	bar->separator_symbol = NULL;
	bar->strip_workspace_numbers = false;
	bar->binding_mode_indicator = true;
	bar->tray_padding = 2;
	// set default colors
	strcpy(bar->colors.background, "#000000ff");
	strcpy(bar->colors.statusline, "#ffffffff");
	strcpy(bar->colors.separator, "#666666ff");
	strcpy(bar->colors.focused_workspace_border, "#4c7899ff");
	strcpy(bar->colors.focused_workspace_bg, "#285577ff");
	strcpy(bar->colors.focused_workspace_text, "#ffffffff");
	strcpy(bar->colors.active_workspace_border, "#333333ff");
	strcpy(bar->colors.active_workspace_bg, "#5f676aff");
	strcpy(bar->colors.active_workspace_text, "#ffffffff");
	strcpy(bar->colors.inactive_workspace_border, "#333333ff");
	strcpy(bar->colors.inactive_workspace_bg,"#222222ff");
	strcpy(bar->colors.inactive_workspace_text, "#888888ff");
	strcpy(bar->colors.urgent_workspace_border, "#2f343aff");
	strcpy(bar->colors.urgent_workspace_bg,"#900000ff");
	strcpy(bar->colors.urgent_workspace_text, "#ffffffff");
	strcpy(bar->colors.binding_mode_border, "#2f343aff");
	strcpy(bar->colors.binding_mode_bg,"#900000ff");
	strcpy(bar->colors.binding_mode_text, "#ffffffff");

	list_add(config->bars, bar);

	return bar;
}
