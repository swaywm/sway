#define _XOPEN_SOURCE 500
#include <string.h>
#include <strings.h>
#include <wlr/util/log.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "util.h"

// Must be in alphabetical order for bsearch
static struct cmd_handler bar_handlers[] = {
	{ "activate_button", bar_cmd_activate_button },
	{ "binding_mode_indicator", bar_cmd_binding_mode_indicator },
	{ "bindsym", bar_cmd_bindsym },
	{ "colors", bar_cmd_colors },
	{ "context_button", bar_cmd_context_button },
	{ "font", bar_cmd_font },
	{ "height", bar_cmd_height },
	{ "hidden_state", bar_cmd_hidden_state },
	{ "icon_theme", bar_cmd_icon_theme },
	{ "id", bar_cmd_id },
	{ "mode", bar_cmd_mode },
	{ "modifier", bar_cmd_modifier },
	{ "output", bar_cmd_output },
	{ "pango_markup", bar_cmd_pango_markup },
	{ "position", bar_cmd_position },
	{ "secondary_button", bar_cmd_secondary_button },
	{ "separator_symbol", bar_cmd_separator_symbol },
	{ "status_command", bar_cmd_status_command },
	{ "strip_workspace_numbers", bar_cmd_strip_workspace_numbers },
	{ "swaybar_command", bar_cmd_swaybar_command },
	{ "tray_output", bar_cmd_tray_output },
	{ "tray_padding", bar_cmd_tray_padding },
	{ "workspace_buttons", bar_cmd_workspace_buttons },
	{ "wrap_scroll", bar_cmd_wrap_scroll },
};

// Must be in alphabetical order for bsearch
static struct cmd_handler bar_config_handlers[] = {
	{ "hidden_state", bar_cmd_hidden_state },
	{ "mode", bar_cmd_mode }
};

struct cmd_results *cmd_bar(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "bar", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (!config->reading) {
		if (!find_handler(argv[0], bar_config_handlers,
					sizeof(bar_config_handlers))) {
			return cmd_results_new(CMD_FAILURE, "bar",
					"Can only be used in config file.");
		}
		return config_subcommand(argv, argc, bar_config_handlers,
				sizeof(bar_config_handlers));
	}

	if (argc > 1) {
		struct bar_config *bar = NULL;
		if (!find_handler(argv[0], bar_handlers, sizeof(bar_handlers))
				&& find_handler(argv[1], bar_handlers, sizeof(bar_handlers))) {
			for (int i = 0; i < config->bars->length; ++i) {
				struct bar_config *item = config->bars->items[i];
				if (strcmp(item->id, argv[0]) == 0) {
					wlr_log(L_DEBUG, "Selecting bar: %s", argv[0]);
					bar = item;
					break;
				}
			}
			if (!bar) {
				wlr_log(L_DEBUG, "Creating bar: %s", argv[0]);
				bar = default_bar_config();
				if (!bar) {
					return cmd_results_new(CMD_FAILURE, "bar",
							"Unable to allocate bar state");
				}

				bar->id = strdup(argv[0]);
			}
			config->current_bar = bar;
			++argv; --argc;
		}
	}

	if (!config->current_bar) {
		// Create new bar with default values
		struct bar_config *bar = default_bar_config();
		if (!bar) {
			return cmd_results_new(CMD_FAILURE, "bar",
					"Unable to allocate bar state");
		}

		// set bar id
		for (int i = 0; i < config->bars->length; ++i) {
			if (bar == config->bars->items[i]) {
				const int len = 5 + numlen(i); // "bar-" + i + \0
				bar->id = malloc(len * sizeof(char));
				if (bar->id) {
					snprintf(bar->id, len, "bar-%d", i);
				} else {
					return cmd_results_new(CMD_FAILURE,
							"bar", "Unable to allocate bar ID");
				}
				break;
			}
		}

		// Set current bar
		config->current_bar = bar;
		wlr_log(L_DEBUG, "Creating bar %s", bar->id);
	}

	return config_subcommand(argv, argc, bar_handlers, sizeof(bar_handlers));
}
