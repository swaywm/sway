#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *bar_cmd_workspace_buttons(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace_buttons", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE,
				"workspace_buttons", "No bar defined.");
	}
	if (strcasecmp("yes", argv[0]) == 0) {
		config->current_bar->workspace_buttons = true;
		sway_log(L_DEBUG, "Enabling workspace buttons on bar: %s",
				config->current_bar->id);
	} else if (strcasecmp("no", argv[0]) == 0) {
		config->current_bar->workspace_buttons = false;
		sway_log(L_DEBUG, "Disabling workspace buttons on bar: %s",
				config->current_bar->id);
	} else {
		return cmd_results_new(CMD_INVALID, "workspace_buttons",
				"Invalid value %s", argv[0]);
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
