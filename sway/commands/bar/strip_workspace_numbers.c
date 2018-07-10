#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *bar_cmd_strip_workspace_numbers(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc,
				"strip_workspace_numbers", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE,
				"strip_workspace_numbers", "No bar defined.");
	}
	if (strcasecmp("yes", argv[0]) == 0) {
		config->current_bar->strip_workspace_numbers = true;
		wlr_log(WLR_DEBUG, "Stripping workspace numbers on bar: %s",
				config->current_bar->id);
	} else if (strcasecmp("no", argv[0]) == 0) {
		config->current_bar->strip_workspace_numbers = false;
		wlr_log(WLR_DEBUG, "Enabling workspace numbers on bar: %s",
				config->current_bar->id);
	} else {
		return cmd_results_new(CMD_INVALID,
				"strip_workspace_numbers", "Invalid value %s", argv[0]);
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
