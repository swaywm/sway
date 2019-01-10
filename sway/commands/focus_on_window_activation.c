#include "sway/commands.h"

struct cmd_results *cmd_focus_on_window_activation(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "focus_on_window_activation",
					EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (strcmp(argv[0], "smart") == 0) {
		config->focus_on_window_activation = FOWA_SMART;
	} else if (strcmp(argv[0], "urgent") == 0) {
		config->focus_on_window_activation = FOWA_URGENT;
	} else if (strcmp(argv[0], "focus") == 0) {
		config->focus_on_window_activation = FOWA_FOCUS;
	} else if (strcmp(argv[0], "none") == 0) {
		config->focus_on_window_activation = FOWA_NONE;
	} else {
		return cmd_results_new(CMD_INVALID,
				"Expected "
				"'focus_on_window_activation smart|urgent|focus|none'");
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
