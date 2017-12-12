#define _XOPEN_SOURCE 700
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "log.h"

struct cmd_results *input_cmd_seat(int argc, char **argv) {
	sway_log(L_DEBUG, "seat for device:  %d %s",
		current_input_config==NULL, current_input_config->identifier);
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "seat", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "seat",
			"No input device defined.");
	}
	struct input_config *new_config =
		new_input_config(current_input_config->identifier);

	// TODO validate seat name
	free(new_config->seat);
	new_config->seat = strdup(argv[0]);

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
