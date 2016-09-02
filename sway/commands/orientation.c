#include <string.h>
#include "sway/commands.h"

struct cmd_results *cmd_orientation(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (!config->reading) return cmd_results_new(CMD_FAILURE, "orientation", "Can only be used in config file.");
	if ((error = checkarg(argc, "orientation", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (strcasecmp(argv[0], "horizontal") == 0) {
		config->default_orientation = L_HORIZ;
	} else if (strcasecmp(argv[0], "vertical") == 0) {
		config->default_orientation = L_VERT;
	} else if (strcasecmp(argv[0], "auto") == 0) {
		// Do nothing
	} else {
		return cmd_results_new(CMD_INVALID, "orientation", "Expected 'orientation <horizontal|vertical|auto>'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
