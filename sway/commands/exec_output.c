#include <fcntl.h>
#include <unistd.h>
#include "sway/commands.h"

struct cmd_results *cmd_exec_output(int argc, char **argv) {
	struct cmd_results *error =
		checkarg(argc, "exec_output", EXPECTED_AT_MOST, 1);
	if (error) {
		return error;
	}

	if (config->exec_out >= 0) {
		if (close(config->exec_out) < 0) {
			return cmd_results_new(
				CMD_FAILURE,
				"Failed to close existing fd %d",
				config->exec_out
			);
		};
		config->exec_out = -1;
	}

	if (argc == 0) {
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	int out = open(argv[0], O_RDWR | O_APPEND | O_CREAT, 0600); // u=rw
	if (out < 0) {
		return cmd_results_new(CMD_FAILURE, "Could not open file %s", argv[0]);
	}
	config->exec_out = out;
	return cmd_results_new(CMD_SUCCESS, NULL);
}
