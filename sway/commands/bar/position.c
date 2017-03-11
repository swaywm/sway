#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *bar_cmd_position(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "position", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "position", "No bar defined.");
	}

	if (strcasecmp("top", argv[0]) == 0) {
		config->current_bar->position = DESKTOP_SHELL_PANEL_POSITION_TOP;
	} else if (strcasecmp("bottom", argv[0]) == 0) {
		config->current_bar->position = DESKTOP_SHELL_PANEL_POSITION_BOTTOM;
	} else if (strcasecmp("left", argv[0]) == 0) {
		sway_log(L_INFO, "Warning: swaybar currently only supports top and bottom positioning. YMMV");
		config->current_bar->position = DESKTOP_SHELL_PANEL_POSITION_LEFT;
	} else if (strcasecmp("right", argv[0]) == 0) {
		sway_log(L_INFO, "Warning: swaybar currently only supports top and bottom positioning. YMMV");
		config->current_bar->position = DESKTOP_SHELL_PANEL_POSITION_RIGHT;
	} else {
		error = cmd_results_new(CMD_INVALID, "position", "Invalid value %s", argv[0]);
		return error;
	}

	sway_log(L_DEBUG, "Setting bar position '%s' for bar: %s", argv[0], config->current_bar->id);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
