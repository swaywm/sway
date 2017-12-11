#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/input/input-manager.h"

struct cmd_results *input_cmd_left_handed(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "left_handed", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "left_handed", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "enabled") == 0) {
		new_config->left_handed = 1;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		new_config->left_handed = 0;
	} else {
		return cmd_results_new(CMD_INVALID, "left_handed", "Expected 'left_handed <enabled|disabled>'");
	}

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
