#include <stdlib.h>
#include <string.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"

struct cmd_results *input_cmd_pointer_accel(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "pointer_accel", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *current_input_config =
		config->handler_context.input_config;
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE,
			"pointer_accel", "No input device defined.");
	}
	struct input_config *new_config =
		new_input_config(current_input_config->identifier);

	float pointer_accel = atof(argv[0]);
	if (pointer_accel < -1 || pointer_accel > 1) {
		free_input_config(new_config);
		return cmd_results_new(CMD_INVALID, "pointer_accel",
			"Input out of range [-1, 1]");
	}
	new_config->pointer_accel = pointer_accel;

	apply_input_config(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
