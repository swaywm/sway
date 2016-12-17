#include <stdbool.h>
#include <string.h>
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
	bool mode_make = (argc == 2 && strcmp(argv[1], "{") == 0);
	if (mode_make) {
		if (!config->reading)
			return cmd_results_new(CMD_FAILURE, "mode", "Can only be used in config file.");
	}
	struct sway_mode *mode = NULL;
	// Find mode
	int i, len = config->modes->length;
	for (i = 0; i < len; ++i) {
		struct sway_mode *find = config->modes->items[i];
		if (strcasecmp(find->name, mode_name) == 0) {
			mode = find;
			break;
		}
	}
	// Create mode if it doesn't exist
	if (!mode && mode_make) {
		mode = malloc(sizeof(struct sway_mode));
		if (!mode) {
			return cmd_results_new(CMD_FAILURE, "mode", "Unable to allocate mode");
		}
		mode->name = strdup(mode_name);
		mode->bindings = create_list();
		list_add(config->modes, mode);
	}
	if (!mode) {
		error = cmd_results_new(CMD_INVALID, "mode", "Unknown mode `%s'", mode_name);
		return error;
	}
	if ((config->reading && mode_make) || (!config->reading && !mode_make)) {
		sway_log(L_DEBUG, "Switching to mode `%s'",mode->name);
	}
	// Set current mode
	config->current_mode = mode;
	if (!mode_make) {
		// trigger IPC mode event
		ipc_event_mode(config->current_mode->name);
	}
	return cmd_results_new(mode_make ? CMD_BLOCK_MODE : CMD_SUCCESS, NULL, NULL);
}
