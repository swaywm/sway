#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "stringop.h"

struct cmd_results *seat_cmd_keyboard_grouping(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "keyboard_grouping", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (!config->handler_context.seat_config) {
		return cmd_results_new(CMD_INVALID, "No seat defined");
	}

	struct seat_config *seat_config = config->handler_context.seat_config;
	if (strcmp(argv[0], "none") == 0) {
		seat_config->keyboard_grouping = KEYBOARD_GROUP_NONE;
	} else if (strcmp(argv[0], "smart") == 0) {
		seat_config->keyboard_grouping = KEYBOARD_GROUP_SMART;
	} else {
		return cmd_results_new(CMD_INVALID,
				"Expected syntax `keyboard_grouping none|smart`");
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
