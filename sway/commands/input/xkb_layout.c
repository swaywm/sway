#define _XOPEN_SOURCE 700
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "log.h"

struct cmd_results *input_cmd_xkb_layout(int argc, char **argv) {
	sway_log(L_DEBUG, "xkb layout for device: %s", current_input_config->identifier);
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "xkb_layout", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "xkb_layout", "No input device defined.");
	}
	struct input_config *new_config =
		new_input_config(current_input_config->identifier);

	new_config->xkb_layout = strdup(argv[0]);

	sway_log(L_DEBUG, "apply-xkb_layout for device: %s layout: %s",
		current_input_config->identifier, new_config->xkb_layout);
	apply_input_config(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
