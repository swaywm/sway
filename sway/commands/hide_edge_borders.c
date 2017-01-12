#include <string.h>
#include "sway/commands.h"

struct cmd_results *cmd_hide_edge_borders(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "hide_edge_borders", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (strcasecmp(argv[0], "none") == 0) {
		config->hide_edge_borders = E_NONE;
	} else if (strcasecmp(argv[0], "vertical") == 0) {
		config->hide_edge_borders = E_VERTICAL;
	} else if (strcasecmp(argv[0], "horizontal") == 0) {
		config->hide_edge_borders = E_HORIZONTAL;
	} else if (strcasecmp(argv[0], "both") == 0) {
		config->hide_edge_borders = E_BOTH;
	} else if (strcasecmp(argv[0], "smart") == 0) {
		config->hide_edge_borders = E_SMART;
	} else {
		return cmd_results_new(CMD_INVALID, "hide_edge_borders",
				"Expected 'hide_edge_borders <none|vertical|horizontal|both>'");
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
