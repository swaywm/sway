#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"

struct cmd_results *input_cmd_accel_profile(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "accel_profile", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *current_input_config =
		config->handler_context.input_config;
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "accel_profile",
				"No input device defined.");
	}
	struct input_config *new_config =
		new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "adaptive") == 0) {
		new_config->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
	} else if (strcasecmp(argv[0], "flat") == 0) {
		new_config->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
	} else {
		free_input_config(new_config);
		return cmd_results_new(CMD_INVALID, "accel_profile",
				"Expected 'accel_profile <adaptive|flat>'");
	}

	apply_input_config(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
