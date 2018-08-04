#include <string.h>
#include "sway/commands.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_swaynag_command(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "swaynag_command", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (config->swaynag_command) {
		free(config->swaynag_command);
	}
	config->swaynag_command = join_args(argv, argc);
	wlr_log(WLR_DEBUG, "Using custom swaynag command: %s",
			config->swaynag_command);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
