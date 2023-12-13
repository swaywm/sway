#include <stdlib.h>
#include <string.h>
#include "log.h"
#include "sway/commands.h"

struct cmd_results *bar_cmd_status_edge_padding(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "status_edge_padding", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	char *end;
	int padding = strtol(argv[0], &end, 10);
	if (strlen(end) || padding < 0) {
		return cmd_results_new(CMD_INVALID,
				"Padding must be a positive integer");
	}
	config->current_bar->status_edge_padding = padding;
	sway_log(SWAY_DEBUG, "Status edge padding on bar %s: %d",
			config->current_bar->id, config->current_bar->status_edge_padding);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
