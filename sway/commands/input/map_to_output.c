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
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	ic->mapped_to = MAPPED_TO_OUTPUT;
	ic->mapped_to_output = strdup(argv[0]);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
