#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"

struct cmd_results *input_cmd_tap_button_map(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "tap_button_map", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *current_input_config =
		config->handler_context.input_config;
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "tap_button_map",
				"No input device defined.");
	}
	struct input_config *new_config =
		new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "lrm") == 0) {
		new_config->tap_button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;
	} else if (strcasecmp(argv[0], "lmr") == 0) {
		new_config->tap_button_map = LIBINPUT_CONFIG_TAP_MAP_LMR;
	} else {
		free_input_config(new_config);
		return cmd_results_new(CMD_INVALID, "tap_button_map",
			"Expected 'tap_button_map <lrm|lmr>'");
	}

	apply_input_config(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
