#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "log.h"

struct cmd_results *input_cmd_tap(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "tap", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *current_input_config =
		config->handler_context.input_config;
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "tap", "No input device defined.");
	}
	struct input_config *new_config =
		new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "enabled") == 0) {
		new_config->tap = LIBINPUT_CONFIG_TAP_ENABLED;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		new_config->tap = LIBINPUT_CONFIG_TAP_DISABLED;
	} else {
		free_input_config(new_config);
		return cmd_results_new(CMD_INVALID, "tap",
			"Expected 'tap <enabled|disabled>'");
	}

	wlr_log(L_DEBUG, "apply-tap for device: %s",
		current_input_config->identifier);
	apply_input_config(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
