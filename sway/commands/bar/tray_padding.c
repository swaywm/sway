#include <stdlib.h>
#include <string.h>
#include "commands.h"
#include "log.h"

struct cmd_results *bar_cmd_tray_padding(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "tray_padding", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "tray_padding", "No bar defined.");
	}

	int padding = atoi(argv[0]);
	if (padding < 0) {
		return cmd_results_new(CMD_INVALID, "tray_padding",
				"Invalid padding value %s, minimum is 0", argv[0]);
	}

	if (argc > 1 && strcasecmp("px", argv[1]) != 0) {
		return cmd_results_new(CMD_INVALID, "tray_padding",
				"Unknown unit %s", argv[1]);
	}
	config->current_bar->tray_padding = padding;
	sway_log(L_DEBUG, "Enabling tray padding of %d px on bar: %s", padding, config->current_bar->id);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
