#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/input/input-manager.h"

struct cmd_results *input_cmd_drag_lock(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "drag_lock", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "drag_lock", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "enabled") == 0) {
		new_config->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_ENABLED;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		new_config->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_DISABLED;
	} else {
		return cmd_results_new(CMD_INVALID, "drag_lock", "Expected 'drag_lock <enabled|disabled>'");
	}

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
