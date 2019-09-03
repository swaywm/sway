#include <string.h>
#include "sway/commands.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *bar_cmd_status_command(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "status_command", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	free(config->current_bar->status_command);
	config->current_bar->status_command = NULL;

	char *new_command = join_args(argv, argc);
	if (strcmp(new_command, "-") != 0) {
		config->current_bar->status_command = new_command;
		sway_log(SWAY_DEBUG, "Feeding bar with status command: %s",
				config->current_bar->status_command);
	} else {
		free(new_command);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
