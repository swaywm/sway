#include <string.h>
#include "sway/commands.h"
#include "sway/input.h"

struct cmd_results *input_cmd_accel_profile(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "accel_profile", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "accel_profile", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "adaptive") == 0) {
		new_config->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
	} else if (strcasecmp(argv[0], "flat") == 0) {
		new_config->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
	} else {
		return cmd_results_new(CMD_INVALID, "accel_profile",
				"Expected 'accel_profile <adaptive|flat>'");
	}

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
