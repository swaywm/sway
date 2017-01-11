#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *bar_cmd_mouse_scroll_workspaces(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "mouse_scroll_workspaces", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "mouse_scroll_workspaces", "No bar defined.");
	}

	if (strcasecmp("yes", argv[0]) == 0) {
		config->current_bar->mouse_scroll_workspaces = true;
		sway_log(L_DEBUG, "Enabling workspace changing by mouse scroll wheel on bar: %s", config->current_bar->id);
	} else if (strcasecmp("no", argv[0]) == 0) {
		config->current_bar->mouse_scroll_workspaces = false;
		sway_log(L_DEBUG, "Disabling workspace changing by mouse scroll wheel on bar: %s", config->current_bar->id);
	} else {
		error = cmd_results_new(CMD_INVALID, "mouse_scroll_workspaces", "Invalid value %s", argv[0]);
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
