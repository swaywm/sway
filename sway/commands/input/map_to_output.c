#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "log.h"

struct cmd_results *input_cmd_map_to_output(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "map_to_output", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct input_config *current_input_config =
		config->handler_context.input_config;
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "map_to_output",
			"No input device defined.");
	}
	struct input_config *new_config =
		new_input_config(current_input_config->identifier);

	new_config->mapped_output = strdup(argv[0]);
	apply_input_config(new_config);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
