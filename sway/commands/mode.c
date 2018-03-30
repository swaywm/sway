#define _XOPEN_SOURCE 500
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/ipc-server.h"
#include "list.h"
#include "log.h"

struct cmd_results *cmd_mode(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "mode", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	const char *mode_name = argv[0];
	bool new_mode = (argc == 2 && strcmp(argv[1], "{") == 0);
	if (new_mode && !config->reading) {
		return cmd_results_new(CMD_FAILURE,
				"mode", "Can only be used in config file.");
	}
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
	if (!mode && new_mode) {
		mode = calloc(1, sizeof(struct sway_mode));
		if (!mode) {
			return cmd_results_new(CMD_FAILURE,
					"mode", "Unable to allocate mode");
		}
		mode->name = strdup(mode_name);
		mode->keysym_bindings = create_list();
		mode->keycode_bindings = create_list();
		list_add(config->modes, mode);
	}
	if (!mode) {
		error = cmd_results_new(CMD_INVALID,
				"mode", "Unknown mode `%s'", mode_name);
		return error;
	}
	if ((config->reading && new_mode) || (!config->reading && !new_mode)) {
		wlr_log(L_DEBUG, "Switching to mode `%s'",mode->name);
	}
	// Set current mode
	config->current_mode = mode;
	if (!new_mode) {
		// trigger IPC mode event
		ipc_event_mode(config->current_mode->name);
	}
	return cmd_results_new(new_mode ? CMD_BLOCK_MODE : CMD_SUCCESS, NULL, NULL);
}
