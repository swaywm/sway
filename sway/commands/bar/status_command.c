#include <string.h>
#include "sway/commands.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *bar_cmd_status_command(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "status_command", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE,
				"status_command", "No bar defined.");
	}
	free(config->current_bar->status_command);
	config->current_bar->status_command = join_args(argv, argc);
	wlr_log(L_DEBUG, "Feeding bar with status command: %s",
			config->current_bar->status_command);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
