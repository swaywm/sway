#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *cmd_include(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "include", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!load_include_configs(argv[0], config)) {
		return cmd_results_new(CMD_INVALID, "include", "Failed to include sub configuration file: %s", argv[0]);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
