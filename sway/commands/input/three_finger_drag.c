#include <libinput.h>
#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "util.h"

struct cmd_results *input_cmd_three_finger_drag(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "three_finger_drag", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	
	if (strcasecmp(argv[0], "three_finger") == 0) {
		ic->drag_3fg = LIBINPUT_CONFIG_3FG_DRAG_ENABLED_3FG;
	} else if (strcasecmp(argv[0], "four_finger") == 0) {
		ic->drag_3fg = LIBINPUT_CONFIG_3FG_DRAG_ENABLED_4FG;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		ic->drag_3fg = LIBINPUT_CONFIG_3FG_DRAG_DISABLED;
	} else {
		return cmd_results_new(CMD_INVALID,
			"Expected 'three_finger_drag <three_finger|four_finger|disabled>'");
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
