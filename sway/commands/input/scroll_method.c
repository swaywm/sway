#include <string.h>
#include "sway/commands.h"
#include "sway/input.h"

struct cmd_results *input_cmd_scroll_method(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "scroll_method", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "scroll_method", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "none") == 0) {
		new_config->scroll_method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
	} else if (strcasecmp(argv[0], "two_finger") == 0) {
		new_config->scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
	} else if (strcasecmp(argv[0], "edge") == 0) {
		new_config->scroll_method = LIBINPUT_CONFIG_SCROLL_EDGE;
	} else if (strcasecmp(argv[0], "on_button_down") == 0) {
		new_config->scroll_method = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
	} else {
		return cmd_results_new(CMD_INVALID, "scroll_method", "Expected 'scroll_method <none|two_finger|edge|on_button_down>'");
	}

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
