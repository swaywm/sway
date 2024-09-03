#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include "sway/commands.h"

struct cmd_results *cmd_env(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "env", EXPECTED_EQUAL_TO, 2))) {
		return error;
	}

	setenv(argv[0], argv[1], 1);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
