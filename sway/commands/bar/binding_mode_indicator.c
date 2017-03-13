#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *bar_cmd_binding_mode_indicator(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "binding_mode_indicator", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "binding_mode_indicator", "No bar defined.");
	}

	if (strcasecmp("yes", argv[0]) == 0) {
		config->current_bar->binding_mode_indicator = true;
		sway_log(L_DEBUG, "Enabling binding mode indicator on bar: %s", config->current_bar->id);
	} else if (strcasecmp("no", argv[0]) == 0) {
		config->current_bar->binding_mode_indicator = false;
		sway_log(L_DEBUG, "Disabling binding mode indicator on bar: %s", config->current_bar->id);
	} else {
		error = cmd_results_new(CMD_INVALID, "binding_mode_indicator", "Invalid value %s", argv[0]);
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
