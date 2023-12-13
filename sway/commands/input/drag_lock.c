#include <string.h>
#include <strings.h>
#include "util.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"

struct cmd_results *input_cmd_drag_lock(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "drag_lock", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	if (parse_boolean(argv[0], true)) {
		ic->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_ENABLED;
	} else {
		ic->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_DISABLED;
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
