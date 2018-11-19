#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "log.h"
#include "util.h"

struct cmd_results *bar_cmd_strip_workspace_name(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc,
				"strip_workspace_name", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE,
				"strip_workspace_name", "No bar defined.");
	}

	config->current_bar->strip_workspace_name =
		parse_boolean(argv[0], config->current_bar->strip_workspace_name);

	if (config->current_bar->strip_workspace_name) {
		config->current_bar->strip_workspace_numbers = false;

		wlr_log(WLR_DEBUG, "Stripping workspace name on bar: %s",
				config->current_bar->id);
	} else {
		wlr_log(WLR_DEBUG, "Enabling workspace name on bar: %s",
				config->current_bar->id);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
