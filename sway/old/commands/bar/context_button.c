#include <stdlib.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *bar_cmd_context_button(int argc, char **argv) {
	const char *cmd_name = "context_button";
#ifndef ENABLE_TRAY
	return cmd_results_new(CMD_INVALID, cmd_name, "Invalid %s command "
			"%s called, but sway was compiled without tray support",
			cmd_name, cmd_name);
#else
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, cmd_name, EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, cmd_name, "No bar defined.");
	}

	// User should be able to prefix with 0x or whatever they want
	config->current_bar->context_button = strtoul(argv[0], NULL, 0);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
#endif
}
