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

	config->current_bar->strip_workspace_name =
		parse_boolean(argv[0], config->current_bar->strip_workspace_name);

	if (config->current_bar->strip_workspace_name) {
		config->current_bar->strip_workspace_numbers = false;

		sway_log(SWAY_DEBUG, "Stripping workspace name on bar: %s",
				config->current_bar->id);
	} else {
		sway_log(SWAY_DEBUG, "Enabling workspace name on bar: %s",
				config->current_bar->id);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
