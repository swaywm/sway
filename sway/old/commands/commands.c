#include <stdbool.h>
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "list.h"
#include "log.h"

struct cmd_results *cmd_commands(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "commands", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if ((error = check_security_config())) {
		return error;
	}

	if (strcmp(argv[0], "{") != 0) {
		return cmd_results_new(CMD_FAILURE, "commands", "Expected block declaration");
	}

	if (!config->reading) {
		return cmd_results_new(CMD_FAILURE, "commands", "Can only be used in config file.");
	}

	return cmd_results_new(CMD_BLOCK_COMMANDS, NULL, NULL);
}
