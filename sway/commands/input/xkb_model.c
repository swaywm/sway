#define _POSIX_C_SOURCE 200809L
#include "sway/config.h"
#include "sway/commands.h"
#include "log.h"

struct cmd_results *input_cmd_xkb_model(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "xkb_model", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	ic->xkb_model = strdup(argv[0]);

	sway_log(SWAY_DEBUG, "set-xkb_model for config: %s model: %s",
			ic->identifier, ic->xkb_model);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
