#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <wordexp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <libinput.h>
#include <limits.h>
#include <float.h>
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
#include "input.h"
#include "border.h"

struct sway_config *config = NULL;

static void terminate_swaybar(pid_t pid);

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

	if (bar->outputs) {
		free_flat_list(bar->outputs);
	}

	if (bar->pid != 0) {
		terminate_swaybar(bar->pid);
	}

	free(bar);
}

void free_input_config(struct input_config *ic) {
	free(ic->identifier);
	free(ic);
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

void free_config(struct sway_config *config) {
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

	for (i = 0; i < config->input_configs->length; ++i) {
		free_input_config(config->input_configs->items[i]);
	}
	list_free(config->input_configs);

	for (i = 0; i < config->output_configs->length; ++i) {
		free_output_config(config->output_configs->items[i]);
	}
	list_free(config->output_configs);

	list_free(config->active_bar_modifiers);
	free_flat_list(config->config_chain);
	free(config->font);
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
	config->input_configs = create_list();
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
	config->floating_scroll = FSB_GAPS_INNER;
	config->default_layout = L_NONE;
	config->default_orientation = L_NONE;
	config->font = strdup("monospace 10");
	config->font_height = get_font_text_height(config->font);

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
	config->smart_gaps = false;
	config->gaps_inner = 0;
	config->gaps_outer = 0;

	config->active_bar_modifiers = create_list();

	config->config_chain = create_list();
	config->current_config = NULL;

	// borders
	config->border = B_NORMAL;
	config->floating_border = B_NORMAL;
	config->border_thickness = 2;
	config->floating_border_thickness = 2;
	config->hide_edge_borders = E_NONE;

	// border colors
	config->border_colors.focused.border = 0x4C7899FF;
	config->border_colors.focused.background = 0x285577FF;
	config->border_colors.focused.text = 0xFFFFFFFF;
	config->border_colors.focused.indicator = 0x2E9EF4FF;
	config->border_colors.focused.child_border = 0x285577FF;

	config->border_colors.focused_inactive.border = 0x333333FF;
	config->border_colors.focused_inactive.background = 0x5F676AFF;
	config->border_colors.focused_inactive.text = 0xFFFFFFFF;
	config->border_colors.focused_inactive.indicator = 0x484E50FF;
	config->border_colors.focused_inactive.child_border = 0x5F676AFF;

	config->border_colors.unfocused.border = 0x333333FF;
	config->border_colors.unfocused.background = 0x222222FF;
	config->border_colors.unfocused.text = 0x888888FF;
	config->border_colors.unfocused.indicator = 0x292D2EFF;
	config->border_colors.unfocused.child_border = 0x222222FF;

	config->border_colors.urgent.border = 0x2F343AFF;
	config->border_colors.urgent.background = 0x900000FF;
	config->border_colors.urgent.text = 0xFFFFFFFF;
	config->border_colors.urgent.indicator = 0x900000FF;
	config->border_colors.urgent.child_border = 0x900000FF;

	config->border_colors.placeholder.border = 0x000000FF;
	config->border_colors.placeholder.background = 0x0C0C0CFF;
	config->border_colors.placeholder.text = 0xFFFFFFFF;
	config->border_colors.placeholder.indicator = 0x000000FF;
	config->border_colors.placeholder.child_border = 0x0C0C0CFF;

	config->border_colors.background = 0xFFFFFFFF;
}

static int compare_modifiers(const void *left, const void *right) {
	uint32_t a = *(uint32_t *)left;
	uint32_t b = *(uint32_t *)right;

	return a - b;
}

void update_active_bar_modifiers() {
	if (config->active_bar_modifiers->length > 0) {
		list_free(config->active_bar_modifiers);
		config->active_bar_modifiers = create_list();
	}

	struct bar_config *bar;
	int i;
	for (i = 0; i < config->bars->length; ++i) {
		bar = config->bars->items[i];
		if (strcmp(bar->mode, "hide") == 0 && strcmp(bar->hidden_state, "hide") == 0) {
			if (list_seq_find(config->active_bar_modifiers, compare_modifiers, &bar->modifier) < 0) {
				list_add(config->active_bar_modifiers, &bar->modifier);
			}
		}
	}
}

static char *get_config_path(void) {
	static const char *config_paths[] = {
		"$HOME/.sway/config",
		"$XDG_CONFIG_HOME/sway/config",
		"$HOME/.i3/config",
		"$XDG_CONFIG_HOME/i3/config",
		SYSCONFDIR "/sway/config",
		SYSCONFDIR "/i3/config",
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

static bool load_config(const char *path, struct sway_config *config) {
	sway_log(L_INFO, "Loading config from %s", path);

	struct stat sb;
	if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
		return false;
	}

	if (path == NULL) {
		sway_log(L_ERROR, "Unable to find a config file!");
		return false;
	}

	FILE *f = fopen(path, "r");
	if (!f) {
		sway_log(L_ERROR, "Unable to open %s for reading", path);
		return false;
	}

	bool config_load_success = read_config(f, config);
	fclose(f);


	if (!config_load_success) {
		sway_log(L_ERROR, "Error(s) loading config!");
	}

	return true;
}

bool load_main_config(const char *file, bool is_active) {
	input_init();

	char *path;
	if (file != NULL) {
		path = strdup(file);
	} else {
		path = get_config_path();
	}

	struct sway_config *old_config = config;
	config = calloc(1, sizeof(struct sway_config));

	config_defaults(config);
	if (is_active) {
		sway_log(L_DEBUG, "Performing configuration file reload");
		config->reloading = true;
		config->active = true;
	}

	config->current_config = path;
	list_add(config->config_chain, path);

	config->reading = true;
	bool success = load_config(path, config);

	if (is_active) {
		config->reloading = false;
	}

	if (old_config) {
		free_config(old_config);
	}
	config->reading = false;

	if (success) {
		update_active_bar_modifiers();
	}

	return success;
}

static bool load_include_config(const char *path, const char *parent_dir, struct sway_config *config) {
	// save parent config
	const char *parent_config = config->current_config;

	char *full_path = strdup(path);
	int len = strlen(path);
	if (len >= 1 && path[0] != '/') {
		len = len + strlen(parent_dir) + 2;
		full_path = malloc(len * sizeof(char));
		snprintf(full_path, len, "%s/%s", parent_dir, path);
	}

	char *real_path = realpath(full_path, NULL);
	free(full_path);

	// check if config has already been included
	int j;
	for (j = 0; j < config->config_chain->length; ++j) {
		char *old_path = config->config_chain->items[j];
		if (strcmp(real_path, old_path) == 0) {
			sway_log(L_DEBUG, "%s already included once, won't be included again.", real_path);
			free(real_path);
			return false;
		}
	}

	config->current_config = real_path;
	list_add(config->config_chain, real_path);
	int index = config->config_chain->length - 1;

	if (!load_config(real_path, config)) {
		free(real_path);
		config->current_config = parent_config;
		list_del(config->config_chain, index);
		return false;
	}

	// restore current_config
	config->current_config = parent_config;
	return true;
}

bool load_include_configs(const char *path, struct sway_config *config) {
	char *wd = getcwd(NULL, 0);
	char *parent_path = strdup(config->current_config);
	const char *parent_dir = dirname(parent_path);

	if (chdir(parent_dir) < 0) {
		free(parent_path);
		free(wd);
		return false;
	}

	wordexp_t p;

	if (wordexp(path, &p, 0) < 0) {
		free(parent_path);
		free(wd);
		return false;
	}

	char **w = p.we_wordv;
	size_t i;
	for (i = 0; i < p.we_wordc; ++i) {
		load_include_config(w[i], parent_dir, config);
	}
	free(parent_path);
	wordfree(&p);

	// restore wd
	if (chdir(wd) < 0) {
		free(wd);
		sway_log(L_ERROR, "failed to restore working directory");
		return false;
	}

	free(wd);
	return true;
}

bool read_config(FILE *file, struct sway_config *config) {
	bool success = true;
	enum cmd_status block = CMD_BLOCK_END;

	int line_number = 0;
	char *line;
	while (!feof(file)) {
		line = read_line(file);
		line_number++;
		line = strip_whitespace(line);
		if (line[0] == '#') {
			free(line);
			continue;
		}
		struct cmd_results *res = config_command(line, block);
		switch(res->status) {
		case CMD_FAILURE:
		case CMD_INVALID:
			sway_log(L_ERROR, "Error on line %i '%s': %s (%s)", line_number, line,
				res->error, config->current_config);
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

		case CMD_BLOCK_INPUT:
			if (block == CMD_BLOCK_END) {
				block = CMD_BLOCK_INPUT;
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

			case CMD_BLOCK_INPUT:
				sway_log(L_DEBUG, "End of input block");
				current_input_config = NULL;
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

	return success;
}

int input_identifier_cmp(const void *item, const void *data) {
	const struct input_config *ic = item;
	const char *identifier = data;
	return strcmp(ic->identifier, identifier);
}

int output_name_cmp(const void *item, const void *data) {
	const struct output_config *output = item;
	const char *name = data;

	return strcmp(output->name, name);
}

void merge_input_config(struct input_config *dst, struct input_config *src) {
	if (src->identifier) {
		if (dst->identifier) {
			free(dst->identifier);
		}
		dst->identifier = strdup(src->identifier);
	}
	if (src->click_method != INT_MIN) {
		dst->click_method = src->click_method;
	}
	if (src->drag_lock != INT_MIN) {
		dst->drag_lock = src->drag_lock;
	}
	if (src->dwt != INT_MIN) {
		dst->dwt = src->dwt;
	}
	if (src->middle_emulation != INT_MIN) {
		dst->middle_emulation = src->middle_emulation;
	}
	if (src->natural_scroll != INT_MIN) {
		dst->natural_scroll = src->natural_scroll;
	}
	if (src->pointer_accel != FLT_MIN) {
		dst->pointer_accel = src->pointer_accel;
	}
	if (src->scroll_method != INT_MIN) {
		dst->scroll_method = src->scroll_method;
	}
	if (src->send_events != INT_MIN) {
		dst->send_events = src->send_events;
	}
	if (src->tap != INT_MIN) {
		dst->tap = src->tap;
	}
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

static void invoke_swaybar(struct bar_config *bar) {
	bar->pid = fork();
	if (bar->pid == 0) {
		if (!bar->swaybar_command) {
			char *const cmd[] = {
				"swaybar",
				"-b",
				bar->id,
				NULL,
			};

			execvp(cmd[0], cmd);
		} else {
			// run custom swaybar
			int len = strlen(bar->swaybar_command) + strlen(bar->id) + 5;
			char *command = malloc(len * sizeof(char));
			snprintf(command, len, "%s -b %s", bar->swaybar_command, bar->id);

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
}

static void terminate_swaybar(pid_t pid) {
	int ret = kill(pid, SIGTERM);
	if (ret != 0) {
		sway_log(L_ERROR, "Unable to terminate swaybar [pid: %d]", pid);
	} else {
		int status;
		waitpid(pid, &status, 0);
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

static bool active_output(const char *name) {
	int i;
	swayc_t *cont = NULL;
	for (i = 0; i < root_container.children->length; ++i) {
		cont = root_container.children->items[i];
		if (cont->type == C_OUTPUT && strcasecmp(name, cont->name) == 0) {
			return true;
		}
	}

	return false;
}

void load_swaybars() {
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
				if (!strcmp(o, "*") || active_output(o)) {
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

	for (i = 0; i < bars->length; ++i) {
		bar = bars->items[i];
		if (bar->pid != 0) {
			terminate_swaybar(bar->pid);
		}
		sway_log(L_DEBUG, "Invoking swaybar for bar id '%s'", bar->id);
		invoke_swaybar(bar);
	}

	list_free(bars);
}

void apply_input_config(struct input_config *ic, struct libinput_device *dev) {
	if (ic) {
		sway_log(L_DEBUG,
			"apply_input_config(%s)",
			ic->identifier);
	}

	if (ic && ic->click_method != INT_MIN) {
		sway_log(L_DEBUG, "apply_input_config(%s) click_set_method(%d)", ic->identifier, ic->click_method);
		libinput_device_config_click_set_method(dev, ic->click_method);
	}
	if (ic && ic->drag_lock != INT_MIN) {
		sway_log(L_DEBUG, "apply_input_config(%s) tap_set_drag_lock_enabled(%d)", ic->identifier, ic->click_method);
		libinput_device_config_tap_set_drag_lock_enabled(dev, ic->drag_lock);
	}
	if (ic && ic->dwt != INT_MIN) {
		sway_log(L_DEBUG, "apply_input_config(%s) dwt_set_enabled(%d)", ic->identifier, ic->dwt);
		libinput_device_config_dwt_set_enabled(dev, ic->dwt);
	}
	if (ic && ic->middle_emulation != INT_MIN) {
		sway_log(L_DEBUG, "apply_input_config(%s) middle_emulation_set_enabled(%d)", ic->identifier, ic->middle_emulation);
		libinput_device_config_middle_emulation_set_enabled(dev, ic->middle_emulation);
	}
	if (ic && ic->natural_scroll != INT_MIN) {
		sway_log(L_DEBUG, "apply_input_config(%s) natural_scroll_set_enabled(%d)", ic->identifier, ic->natural_scroll);
		libinput_device_config_scroll_set_natural_scroll_enabled(dev, ic->natural_scroll);
	}
	if (ic && ic->pointer_accel != FLT_MIN) {
		sway_log(L_DEBUG, "apply_input_config(%s) accel_set_speed(%f)", ic->identifier, ic->pointer_accel);
		libinput_device_config_accel_set_speed(dev, ic->pointer_accel);
	}
	if (ic && ic->scroll_method != INT_MIN) {
		sway_log(L_DEBUG, "apply_input_config(%s) scroll_set_method(%d)", ic->identifier, ic->scroll_method);
		libinput_device_config_scroll_set_method(dev, ic->scroll_method);
	}
	if (ic && ic->send_events != INT_MIN) {
		sway_log(L_DEBUG, "apply_input_config(%s) send_events_set_mode(%d)", ic->identifier, ic->send_events);
		libinput_device_config_send_events_set_mode(dev, ic->send_events);
	}
	if (ic && ic->tap != INT_MIN) {
		sway_log(L_DEBUG, "apply_input_config(%s) tap_set_enabled(%d)", ic->identifier, ic->tap);
		libinput_device_config_tap_set_enabled(dev, ic->tap);
	}
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

	// reload swaybars
	load_swaybars();
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
	struct wlc_modifiers no_mods = { 0, 0 };
	for (int i = 0; i < binda->keys->length; i++) {
		xkb_keysym_t ka = *(xkb_keysym_t *)binda->keys->items[i],
			kb = *(xkb_keysym_t *)bindb->keys->items[i];
		if (binda->bindcode) {
			uint32_t *keycode = binda->keys->items[i];
			ka = wlc_keyboard_get_keysym_for_key(*keycode, &no_mods);
		}

		if (bindb->bindcode) {
			uint32_t *keycode = bindb->keys->items[i];
			kb = wlc_keyboard_get_keysym_for_key(*keycode, &no_mods);
		}

		if (ka > kb) {
			return 1;
		} else if (ka < kb) {
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

int sway_binding_cmp_qsort(const void *a, const void *b) {
	return sway_binding_cmp(*(void **)a, *(void **)b);
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

int sway_mouse_binding_cmp_qsort(const void *a, const void *b) {
	return sway_mouse_binding_cmp(*(void **)a, *(void **)b);
}

void free_sway_mouse_binding(struct sway_mouse_binding *binding) {
	if (binding->command) {
		free(binding->command);
	}
	free(binding);
}

struct sway_binding *sway_binding_dup(struct sway_binding *sb) {
	struct sway_binding *new_sb = malloc(sizeof(struct sway_binding));

	new_sb->order = sb->order;
	new_sb->modifiers = sb->modifiers;
	new_sb->command = strdup(sb->command);

	new_sb->keys = create_list();
	int i;
	for (i = 0; i < sb->keys->length; ++i) {
		xkb_keysym_t *key = malloc(sizeof(xkb_keysym_t));
		*key = *(xkb_keysym_t *)sb->keys->items[i];
		list_add(new_sb->keys, key);
	}

	return new_sb;
}

struct bar_config *default_bar_config(void) {
	struct bar_config *bar = NULL;
	bar = malloc(sizeof(struct bar_config));
	bar->mode = strdup("dock");
	bar->hidden_state = strdup("hide");
	bar->modifier = WLC_BIT_MOD_LOGO;
	bar->outputs = NULL;
	bar->position = DESKTOP_SHELL_PANEL_POSITION_BOTTOM;
	bar->bindings = create_list();
	bar->status_command = strdup("while :; do date +'%Y-%m-%d %l:%M:%S %p' && sleep 1; done");
	bar->pango_markup = true;
	bar->swaybar_command = NULL;
	bar->font = NULL;
	bar->height = -1;
	bar->workspace_buttons = true;
	bar->separator_symbol = NULL;
	bar->strip_workspace_numbers = false;
	bar->binding_mode_indicator = true;
	bar->tray_padding = 2;
	bar->pid = 0;
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
