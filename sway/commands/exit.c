#include <stddef.h>
#include "sway/commands.h"
#include "sway/config.h"

void sway_terminate(int exit_code);

struct cmd_results cmd_exit(int argc, char **argv) {
	struct cmd_results error;
	if (checkarg(&error, argc, "exit", EXPECTED_EQUAL_TO, 0)) {
		return error;
	}
	sway_terminate(0);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
