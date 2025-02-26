#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include "sway/commands.h"
#include "env.h"

extern char **child_envp;

struct cmd_results *cmd_env(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "env", EXPECTED_EQUAL_TO, 2))) {
		return error;
	}

	child_envp = env_setenv(child_envp, argv[0], argv[1]);

	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_env_unset(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "env_unset", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	child_envp = env_unsetenv(child_envp, argv[0]);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
