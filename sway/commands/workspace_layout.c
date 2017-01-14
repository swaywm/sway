#include <string.h>
#include "sway/commands.h"

struct cmd_results *cmd_workspace_layout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace_layout", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (strcasecmp(argv[0], "default") == 0) {
		config->default_layout = L_NONE;
	} else if (strcasecmp(argv[0], "stacking") == 0) {
		config->default_layout = L_STACKED;
	} else if (strcasecmp(argv[0], "tabbed") == 0) {
		config->default_layout = L_TABBED;
	} else if (strcasecmp(argv[0], "auto") == 0) {
		if (argc == 1) {
			config->default_layout = L_AUTO_FIRST;
		} else {
			if ((error = checkarg(argc, "workspace_layout auto", EXPECTED_EQUAL_TO, 2))) {
				return error;
			}
			if (strcasecmp(argv[0], "left") == 0) {
				config->default_layout = L_AUTO_LEFT;
			} else if (strcasecmp(argv[0], "right") == 0) {
				config->default_layout = L_AUTO_RIGHT;
			} else if (strcasecmp(argv[0], "top") == 0) {
				config->default_layout = L_AUTO_TOP;
			} else if (strcasecmp(argv[0], "bottom") == 0) {
				config->default_layout = L_AUTO_BOTTOM;
			} else {
				return cmd_results_new(CMD_INVALID, "workspace_layout auto", "Expected 'workspace_layout auto <left|right|top|bottom>'");
			}
		}
	} else {
		return cmd_results_new(CMD_INVALID, "workspace_layout", "Expected 'workspace_layout <default|stacking|tabbed|auto|auto left|auto right|auto top|auto bottom>'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
