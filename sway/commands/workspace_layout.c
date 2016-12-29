#include <string.h>
#include "sway/commands.h"

struct cmd_results *cmd_workspace_layout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace_layout", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (strcasecmp(argv[0], "default") == 0) {
		config->default_layout = L_NONE;
	} else if (strcasecmp(argv[0], "stacking") == 0) {
		config->default_layout = L_STACKED;
	} else if (strcasecmp(argv[0], "tabbed") == 0) {
		config->default_layout = L_TABBED;
	} else if (strcasecmp(argv[0], "auto_left") == 0) {
		config->default_layout = L_AUTO_LEFT;
	} else if (strcasecmp(argv[0], "auto_right") == 0) {
		config->default_layout = L_AUTO_RIGHT;
	} else if (strcasecmp(argv[0], "auto_top") == 0) {
		config->default_layout = L_AUTO_TOP;
	} else if (strcasecmp(argv[0], "auto_bottom") == 0) {
		config->default_layout = L_AUTO_BOTTOM;
	} else {
		return cmd_results_new(CMD_INVALID, "workspace_layout", "Expected 'workspace_layout <default|stacking|tabbed|auto_left|auto_right|auto_top|auto_bottom>'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
