#include <string.h>
#include "commands.h"
#include "log.h"

struct cmd_results *bar_cmd_wrap_scroll(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "wrap_scroll", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "wrap_scroll", "No bar defined.");
	}

	if (strcasecmp("yes", argv[0]) == 0) {
		config->current_bar->wrap_scroll = true;
		sway_log(L_DEBUG, "Enabling wrap scroll on bar: %s", config->current_bar->id);
	} else if (strcasecmp("no", argv[0]) == 0) {
		config->current_bar->wrap_scroll = false;
		sway_log(L_DEBUG, "Disabling wrap scroll on bar: %s", config->current_bar->id);
	} else {
		error = cmd_results_new(CMD_INVALID, "wrap_scroll", "Invalid value %s", argv[0]);
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
