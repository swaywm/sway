#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"
#include "util.h"

struct cmd_results *cmd_smart_borders(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "smart_borders", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	enum edge_border_types saved = config->hide_edge_borders;
	if (strcmp(argv[0], "no_gaps") == 0) {
		config->hide_edge_borders = E_SMART_NO_GAPS;
	} else {
		config->hide_edge_borders = parse_boolean(argv[0], true) ?
			E_SMART : config->saved_edge_borders;
	}
	config->saved_edge_borders = saved;

	arrange_root();

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
