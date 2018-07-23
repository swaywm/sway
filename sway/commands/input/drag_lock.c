#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "util.h"

struct cmd_results *input_cmd_drag_lock(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "drag_lock", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *current_input_config =
		config->handler_context.input_config;
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE,
			"drag_lock", "No input device defined.");
	}
	struct input_config *new_config =
		new_input_config(current_input_config->identifier);

	if (parse_boolean(argv[0], true)) {
		new_config->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_ENABLED;
	} else {
		new_config->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_DISABLED;
	}

	apply_input_config(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
