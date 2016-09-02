#include <stdlib.h>
#include <string.h>
#include "commands.h"
#include "input.h"

struct cmd_results *input_cmd_pointer_accel(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "pointer_accel", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "pointer_accel", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	float pointer_accel = atof(argv[0]);
	if (pointer_accel < -1 || pointer_accel > 1) {
		return cmd_results_new(CMD_INVALID, "pointer_accel", "Input out of range [-1, 1]");
	}
	new_config->pointer_accel = pointer_accel;

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
