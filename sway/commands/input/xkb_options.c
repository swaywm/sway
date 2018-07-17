#define _XOPEN_SOURCE 700
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "log.h"

struct cmd_results *input_cmd_xkb_options(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "xkb_options", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct input_config *current_input_config =
		config->handler_context.input_config;
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "xkb_options", "No input device defined.");
	}
	struct input_config *new_config =
		new_input_config(current_input_config->identifier);

	new_config->xkb_options = strdup(argv[0]);

	wlr_log(WLR_DEBUG, "apply-xkb_options for device: %s options: %s",
		current_input_config->identifier, new_config->xkb_options);
	apply_input_config(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
