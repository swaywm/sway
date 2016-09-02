#include "sway/commands.h"
#include "sway/container.h"

void sway_terminate(int exit_code);

struct cmd_results *cmd_exit(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "exit", "Can't be used in config file.");
	if ((error = checkarg(argc, "exit", EXPECTED_EQUAL_TO, 0))) {
		return error;
	}
	// Close all views
	close_views(&root_container);
	sway_terminate(EXIT_SUCCESS);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

