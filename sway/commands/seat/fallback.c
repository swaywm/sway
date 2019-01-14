#include "sway/config.h"
#include "sway/commands.h"
#include "util.h"

struct cmd_results *seat_cmd_fallback(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "fallback", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (!config->handler_context.seat_config) {
		return cmd_results_new(CMD_FAILURE, "No seat defined");
	}

	config->handler_context.seat_config->fallback =
		parse_boolean(argv[0], false);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
