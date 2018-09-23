#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"

struct cmd_results *input_cmd_xkb_capslock(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "xkb_capslock", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "xkb_capslock", 
			"No input device defined.");
	}

	if (strcasecmp(argv[0], "enabled") == 0) {
		ic->xkb_capslock = 1;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		ic->xkb_capslock = 0;
	} else {
		return cmd_results_new(CMD_INVALID, "xkb_capslock",
			"Expected 'xkb_capslock <enabled|disabled>'");
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
