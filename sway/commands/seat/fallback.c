#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "util.h"

struct cmd_results *seat_cmd_fallback(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "fallback", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct seat_config *current_seat_config =
		config->handler_context.seat_config;
	if (!current_seat_config) {
		return cmd_results_new(CMD_FAILURE, "fallback", "No seat defined");
	}
	struct seat_config *new_config =
		new_seat_config(current_seat_config->name);
		
	new_config->fallback = parse_boolean(argv[0], false);

	if (!config->validating) {
		apply_seat_config(new_config);
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
