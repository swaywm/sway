#define _XOPEN_SOURCE 500
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/ipc-server.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

// Must be in order for the bsearch
static struct cmd_handler mode_handlers[] = {
	{ "bindcode", cmd_bindcode },
	{ "bindsym", cmd_bindsym }
};

struct cmd_results *cmd_mode(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "mode", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (argc > 1 && !config->reading) {
		return cmd_results_new(CMD_FAILURE,
				"mode", "Can only be used in config file.");
	}

	bool pango = strcmp(*argv, "--pango_markup") == 0;
	if (pango) {
		argc--; argv++;
		if (argc == 0) {
			return cmd_results_new(CMD_FAILURE, "mode",
					"Mode name is missing");
		}
	}

	char *mode_name = *argv;
	strip_quotes(mode_name);
	struct sway_mode *mode = NULL;
	// Find mode
	for (int i = 0; i < config->modes->length; ++i) {
		struct sway_mode *test = config->modes->items[i];
		if (strcasecmp(test->name, mode_name) == 0) {
			mode = test;
			break;
		}
	}
	// Create mode if it doesn't exist
	if (!mode && argc > 1) {
		mode = calloc(1, sizeof(struct sway_mode));
		if (!mode) {
			return cmd_results_new(CMD_FAILURE,
					"mode", "Unable to allocate mode");
		}
		mode->name = strdup(mode_name);
		mode->keysym_bindings = create_list();
		mode->keycode_bindings = create_list();
		mode->pango = pango;
		list_add(config->modes, mode);
	}
	if (!mode) {
		error = cmd_results_new(CMD_INVALID,
				"mode", "Unknown mode `%s'", mode_name);
		return error;
	}
	if ((config->reading && argc > 1) || (!config->reading && argc == 1)) {
		wlr_log(L_DEBUG, "Switching to mode `%s' (pango=%d)",
				mode->name, mode->pango);
	}
	// Set current mode
	config->current_mode = mode;
	if (argc == 1) {
		// trigger IPC mode event
		ipc_event_mode(config->current_mode->name,
				config->current_mode->pango);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}

	// Create binding
	struct cmd_results *result = config_subcommand(argv + 1, argc - 1,
			mode_handlers, sizeof(mode_handlers));
	config->current_mode = config->modes->items[0];

	return result;
}
