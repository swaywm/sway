#define _POSIX_C_SOURCE 200809L
#include "sway/config.h"
#include "sway/commands.h"
#include "log.h"

struct cmd_results *input_cmd_xkb_layout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "xkb_layout", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	ic->xkb_layout = strdup(argv[0]);

	sway_log(SWAY_DEBUG, "set-xkb_layout for config: %s layout: %s",
			ic->identifier, ic->xkb_layout);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
