#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "log.h"
#include "util.h"

struct cmd_results *bar_cmd_workspace_buttons(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace_buttons", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE,
				"workspace_buttons", "No bar defined.");
	}
	config->current_bar->workspace_buttons = 
		parse_boolean(argv[0], config->current_bar->workspace_buttons);
	if (config->current_bar->workspace_buttons) {
		wlr_log(WLR_DEBUG, "Enabling workspace buttons on bar: %s",
				config->current_bar->id);
	} else {
		wlr_log(WLR_DEBUG, "Disabling workspace buttons on bar: %s",
				config->current_bar->id);
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
