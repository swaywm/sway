#include "sway/commands.h"
#include "sway/config.h"
#include "sway/layout.h"

struct cmd_results *cmd_reload(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "reload", EXPECTED_EQUAL_TO, 0))) {
		return error;
	}
	if (!load_main_config(config->current_config, true)) {
		return cmd_results_new(CMD_FAILURE, "reload", "Error(s) reloading config.");
	}

	/* load_swaybars(); -- for when it's implemented */

	arrange_windows(&root_container, -1, -1);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
