#include <stdlib.h>
#include <strings.h>
#include "config.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"

struct cmd_results *bar_cmd_workspace_min_width(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace_min_width", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	struct bar_config *bar = config->current_bar;

	char *end;
	int min_width = strtol(argv[0], &end, 10);
	if (min_width < 0 || (*end != '\0' && strcasecmp(end, "px") != 0)) {
		return cmd_results_new(CMD_INVALID,
				"[Bar %s] Invalid minimum workspace button width value: %s",
				bar->id, argv[0]);
	}

	if (argc == 2 && strcasecmp(argv[1], "px") != 0) {
		return cmd_results_new(CMD_INVALID,
				"Expected 'workspace_min_width <px> [px]'");
	}

	sway_log(SWAY_DEBUG, "[Bar %s] Setting minimum workspace button width to %d",
			bar->id, min_width);
	config->current_bar->workspace_min_width = min_width;
	return cmd_results_new(CMD_SUCCESS, NULL);
}
