#include <string.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *cmd_log_colors(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (!config->reading) return cmd_results_new(CMD_FAILURE, "log_colors", "Can only be used in config file.");
	if ((error = checkarg(argc, "log_colors", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (strcasecmp(argv[0], "no") == 0) {
		sway_log_colors(0);
	} else if (strcasecmp(argv[0], "yes") == 0) {
		sway_log_colors(1);
	} else {
		error = cmd_results_new(CMD_FAILURE, "log_colors",
			"Invalid log_colors command (expected `yes` or `no`, got '%s')", argv[0]);
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
