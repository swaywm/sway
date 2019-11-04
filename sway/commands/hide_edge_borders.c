#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"

struct cmd_results *cmd_hide_edge_borders(int argc, char **argv) {
	const char *expected_syntax = "Expected 'hide_edge_borders [--i3] "
		"none|vertical|horizontal|both|smart|smart_no_gaps";

	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "hide_edge_borders", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	bool hide_lone_tab = false;
	if (strcmp(*argv, "--i3") == 0) {
		hide_lone_tab = true;
		++argv;
		--argc;
	}

	if (!argc) {
		return cmd_results_new(CMD_INVALID, expected_syntax);
	}

	if (strcmp(argv[0], "none") == 0) {
		config->hide_edge_borders = E_NONE;
	} else if (strcmp(argv[0], "vertical") == 0) {
		config->hide_edge_borders = E_VERTICAL;
	} else if (strcmp(argv[0], "horizontal") == 0) {
		config->hide_edge_borders = E_HORIZONTAL;
	} else if (strcmp(argv[0], "both") == 0) {
		config->hide_edge_borders = E_BOTH;
	} else if (strcmp(argv[0], "smart") == 0) {
		config->hide_edge_borders = E_NONE;
		config->hide_edge_borders_smart = ESMART_ON;
	} else if (strcmp(argv[0], "smart_no_gaps") == 0) {
		config->hide_edge_borders = E_NONE;
		config->hide_edge_borders_smart = ESMART_NO_GAPS;
	} else {
		return cmd_results_new(CMD_INVALID, expected_syntax);
	}
	config->hide_lone_tab = hide_lone_tab;

	arrange_root();

	return cmd_results_new(CMD_SUCCESS, NULL);
}
