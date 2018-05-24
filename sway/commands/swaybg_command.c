#include <string.h>
#include "sway/commands.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_swaybg_command(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "swaybg_command", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (config->swaybg_command) {
		free(config->swaybg_command);
	}
	config->swaybg_command = join_args(argv, argc);
	sway_log(L_DEBUG, "Using custom swaybg command: %s",
			config->swaybg_command);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
