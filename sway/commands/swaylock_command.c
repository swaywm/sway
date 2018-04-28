#include <string.h>
#include "sway/commands.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_swaylock_command(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "swaylock_command", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	free(config->swaylock_command);
	config->swaylock_command = join_args(argv, argc);
	wlr_log(L_DEBUG, "Using custom swaylock command: %s",
			config->swaylock_command);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
