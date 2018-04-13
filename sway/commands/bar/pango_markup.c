#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *bar_cmd_pango_markup(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "pango_markup", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "pango_markup", "No bar defined.");
	}
	if (strcasecmp("enabled", argv[0]) == 0) {
		config->current_bar->pango_markup = true;
		wlr_log(L_DEBUG, "Enabling pango markup for bar: %s",
				config->current_bar->id);
	} else if (strcasecmp("disabled", argv[0]) == 0) {
		config->current_bar->pango_markup = false;
		wlr_log(L_DEBUG, "Disabling pango markup for bar: %s",
				config->current_bar->id);
	} else {
		error = cmd_results_new(CMD_INVALID, "pango_markup",
				"Invalid value %s", argv[0]);
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
