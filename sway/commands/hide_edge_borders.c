#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"

static void _configure_view(struct sway_container *con, void *data) {
	if (con->type == C_VIEW) {
		view_autoconfigure(con->sway_view);
	}
}

struct cmd_results *cmd_hide_edge_borders(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "hide_edge_borders", EXPECTED_EQUAL_TO, 1))) {
		return error;
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
		config->hide_edge_borders = E_SMART;
	} else {
		return cmd_results_new(CMD_INVALID, "hide_edge_borders",
				"Expected 'hide_edge_borders "
				"<none|vertical|horizontal|both|smart>'");
	}

	container_for_each_descendant_dfs(&root_container, _configure_view, NULL);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
