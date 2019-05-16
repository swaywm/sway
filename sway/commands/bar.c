#define _POSIX_C_SOURCE 200809
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"

// Must be in alphabetical order for bsearch
static struct cmd_handler bar_handlers[] = {
	{ "bindcode", bar_cmd_bindcode },
	{ "binding_mode_indicator", bar_cmd_binding_mode_indicator },
	{ "bindsym", bar_cmd_bindsym },
	{ "colors", bar_cmd_colors },
	{ "font", bar_cmd_font },
	{ "gaps", bar_cmd_gaps },
	{ "height", bar_cmd_height },
	{ "hidden_state", bar_cmd_hidden_state },
	{ "icon_theme", bar_cmd_icon_theme },
	{ "mode", bar_cmd_mode },
	{ "modifier", bar_cmd_modifier },
	{ "output", bar_cmd_output },
	{ "pango_markup", bar_cmd_pango_markup },
	{ "position", bar_cmd_position },
	{ "separator_symbol", bar_cmd_separator_symbol },
	{ "status_command", bar_cmd_status_command },
	{ "status_edge_padding", bar_cmd_status_edge_padding },
	{ "status_padding", bar_cmd_status_padding },
	{ "strip_workspace_name", bar_cmd_strip_workspace_name },
	{ "strip_workspace_numbers", bar_cmd_strip_workspace_numbers },
	{ "tray_bindcode", bar_cmd_tray_bindcode },
	{ "tray_bindsym", bar_cmd_tray_bindsym },
	{ "tray_output", bar_cmd_tray_output },
	{ "tray_padding", bar_cmd_tray_padding },
	{ "unbindcode", bar_cmd_unbindcode },
	{ "unbindsym", bar_cmd_unbindsym },
	{ "workspace_buttons", bar_cmd_workspace_buttons },
	{ "wrap_scroll", bar_cmd_wrap_scroll },
};

// Must be in alphabetical order for bsearch
static struct cmd_handler bar_config_handlers[] = {
	{ "id", bar_cmd_id },
	{ "swaybar_command", bar_cmd_swaybar_command },
};

// Determines whether the subcommand is valid in any bar handler struct
static bool is_subcommand(char *name) {
	return find_handler(name, bar_handlers, sizeof(bar_handlers)) ||
		find_handler(name, bar_config_handlers, sizeof(bar_config_handlers));
}

struct cmd_results *cmd_bar(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "bar", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	bool spawn = false;
	struct bar_config *bar = NULL;
	if (strcmp(argv[0], "id") != 0 && is_subcommand(argv[1])) {
		for (int i = 0; i < config->bars->length; ++i) {
			struct bar_config *item = config->bars->items[i];
			if (strcmp(item->id, argv[0]) == 0) {
				sway_log(SWAY_DEBUG, "Selecting bar: %s", argv[0]);
				bar = item;
				break;
			}
		}
		if (!bar) {
			spawn = !config->reading;
			sway_log(SWAY_DEBUG, "Creating bar: %s", argv[0]);
			bar = default_bar_config();
			if (!bar) {
				return cmd_results_new(CMD_FAILURE,
						"Unable to allocate bar state");
			}

			bar->id = strdup(argv[0]);
		}
		config->current_bar = bar;
		++argv; --argc;
	}

	if (!config->current_bar) {
		if (config->reading) {
			// Create new bar with default values
			struct bar_config *bar = default_bar_config();
			if (!bar) {
				return cmd_results_new(CMD_FAILURE,
						"Unable to allocate bar state");
			}

			// set bar id
			int len = snprintf(NULL, 0, "bar-%d", config->bars->length - 1) + 1;
			bar->id = malloc(len * sizeof(char));
			if (bar->id) {
				snprintf(bar->id, len, "bar-%d", config->bars->length - 1);
			} else {
				return cmd_results_new(CMD_FAILURE, "Unable to allocate bar ID");
			}

			// Set current bar
			config->current_bar = bar;
			sway_log(SWAY_DEBUG, "Creating bar %s", bar->id);
		} else {
			return cmd_results_new(CMD_FAILURE, "No bar defined.");
		}
	}

	if (find_handler(argv[0], bar_config_handlers,
				sizeof(bar_config_handlers))) {
		if (config->reading) {
			return config_subcommand(argv, argc, bar_config_handlers,
					sizeof(bar_config_handlers));
		} else if (spawn) {
			for (int i = config->bars->length - 1; i >= 0; i--) {
				struct bar_config *bar = config->bars->items[i];
				if (bar == config->current_bar) {
					list_del(config->bars, i);
					free_bar_config(bar);
					break;
				}
			}
		}
		return cmd_results_new(CMD_INVALID,
				"Can only be used in the config file.");
	}

	struct cmd_results *res =
		config_subcommand(argv, argc, bar_handlers, sizeof(bar_handlers));
	if (!config->reading) {
		if (spawn) {
			load_swaybar(config->current_bar);
		}
		config->current_bar = NULL;
	}
	return res;
}
