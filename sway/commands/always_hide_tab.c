#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"

struct cmd_results *cmd_always_hide_tab(int argc, char **argv) {
	const char *expected_syntax = "Expected 'always_hide_tab "
		"yes|no";

	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "always_hide_tab", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (!argc) {
		return cmd_results_new(CMD_INVALID, expected_syntax);
	}

	if (strcmp(argv[0], "yes") == 0) {
		config->always_hide_tab = true;
	} else if (strcmp(argv[0], "no") == 0) {
		config->always_hide_tab = false;
	} else {
		return cmd_results_new(CMD_INVALID, expected_syntax);
	}
	arrange_root();

	return cmd_results_new(CMD_SUCCESS, NULL);
}
