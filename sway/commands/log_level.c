#include <stdlib.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *cmd_log_level(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "log_level", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	sway_log_importance_t verbosity;
	if (strcmp(*argv, "debug") == 0) {
		verbosity = SWAY_DEBUG;
	} else if (strcmp(*argv, "info") == 0) {
		verbosity = SWAY_INFO;
	} else if (strcmp(*argv, "error") == 0) {
		verbosity = SWAY_ERROR;
	} else if (strcmp(*argv, "silent") == 0) {
		verbosity = SWAY_SILENT;
	} else {
		return cmd_results_new(CMD_INVALID, "Invalid log_level option.");
	}

	sway_log_init(verbosity, NULL);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
