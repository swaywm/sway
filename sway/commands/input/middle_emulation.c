#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"

struct cmd_results *input_cmd_middle_emulation(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "middle_emulation", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "middle_emulation",
			"No input device defined.");
	}
	struct input_config *new_config =
		new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "enabled") == 0) {
		new_config->middle_emulation = LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		new_config->middle_emulation =
			LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED;
	} else {
		return cmd_results_new(CMD_INVALID, "middle_emulation",
			"Expected 'middle_emulation <enabled|disabled>'");
	}

	apply_input_config(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
