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

	if (strcmp(argv[0], "no_gaps") == 0) {
		config->hide_edge_borders_smart = ESMART_NO_GAPS;
	} else {
		config->hide_edge_borders_smart = parse_boolean(argv[0], true) ?
			ESMART_ON : ESMART_OFF;
	}

	arrange_root();

	return cmd_results_new(CMD_SUCCESS, NULL);
}
