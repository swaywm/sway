#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <glib.h>
#include "sway/commands.h"

extern char **child_envp;

struct cmd_results *cmd_env(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "env", EXPECTED_EQUAL_TO, 2))) {
		return error;
	}

	child_envp = g_environ_setenv(child_envp, argv[0], argv[1], 1);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
