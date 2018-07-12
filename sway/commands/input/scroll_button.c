#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"

struct cmd_results *input_cmd_scroll_button(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "scroll_button", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *current_input_config =
		config->handler_context.input_config;
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "scroll_button",
			"No input device defined.");
	}
	struct input_config *new_config =
		new_input_config(current_input_config->identifier);

	int scroll_button = atoi(argv[0]);
	if (scroll_button < 0 || scroll_button > 1000) {
		free_input_config(new_config);
		return cmd_results_new(CMD_INVALID, "scroll_button",
				"Input out of range [1, 10]");
	}
	new_config->scroll_button = scroll_button;

	apply_input_config(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
