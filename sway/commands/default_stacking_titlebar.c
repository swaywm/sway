#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/container.h"

struct cmd_results *cmd_default_stacking_titlebar(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "default_stacking_titlebar", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (strcmp(argv[0], "always_visible") == 0) {
		config->stacking_titlebar_follows_border = false;
	} else if (strcmp(argv[0], "follows_border") == 0) {
		config->stacking_titlebar_follows_border = true;
	} else {
		return cmd_results_new(CMD_INVALID,
				"Expected 'default_stacking_titlebar <always_visible|follows_border>");
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_default_tabbed_titlebar(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "default_tabbed_titlebar", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (strcmp(argv[0], "always_visible") == 0) {
		config->tabbed_titlebar_follows_border = false;
	} else if (strcmp(argv[0], "follows_border") == 0) {
		config->tabbed_titlebar_follows_border = true;
	} else {
		return cmd_results_new(CMD_INVALID,
				"Expected 'default_tabbed_titlebar <always_visible|follows_border>");
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
