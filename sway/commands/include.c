#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *cmd_include(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "include", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	char *errors = NULL;
	if (!load_include_configs(argv[0], config, &errors)) {
		struct cmd_results *result = cmd_results_new(CMD_INVALID, "include",
				"Failed to include sub configuration file: %s", argv[0]);
		free(errors);
		return result;
	}

	if (errors) {
		struct cmd_results *result = cmd_results_new(CMD_INVALID, "include",
				"There are errors in the included config\n%s", errors);
		free(errors);
		return result;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
