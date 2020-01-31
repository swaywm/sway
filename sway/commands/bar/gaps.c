#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/ipc-server.h"
#include "log.h"

struct cmd_results *bar_cmd_gaps(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "gaps", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if ((error = checkarg(argc, "gaps", EXPECTED_AT_MOST, 4))) {
		return error;
	}

	int top = 0, right = 0, bottom = 0, left = 0;

	for (int i = 0; i < argc; i++) {
		char *end;
		int amount = strtol(argv[i], &end, 10);
		if (strlen(end) && strcasecmp(end, "px") != 0) {
			return cmd_results_new(CMD_INVALID,
					"Expected 'bar [<bar-id>] gaps <all> | <horizontal> "
					"<vertical> | <top> <right> <bottom> <left>'");
		}

		if (i == 0) {
			top = amount;
		}
		if (i == 0 || i == 1) {
			right = amount;
		}
		if (i == 0 || i == 2) {
			bottom = amount;
		}
		if (i == 0 || i == 1 || i == 3) {
			left = amount;
		}
	}

	config->current_bar->gaps.top = top;
	config->current_bar->gaps.right = right;
	config->current_bar->gaps.bottom = bottom;
	config->current_bar->gaps.left = left;

	sway_log(SWAY_DEBUG, "Setting bar gaps to %d %d %d %d on bar: %s",
			config->current_bar->gaps.top, config->current_bar->gaps.right,
			config->current_bar->gaps.bottom, config->current_bar->gaps.left,
			config->current_bar->id);

	if (!config->reading) {
		ipc_event_barconfig_update(config->current_bar);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
