#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *bar_cmd_position(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "position", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "position", "No bar defined.");
	}

	char *valid[] = { "top", "bottom", "left", "right" };
	for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); ++i) {
		if (strcasecmp(valid[i], argv[0]) == 0) {
			wlr_log(L_DEBUG, "Setting bar position '%s' for bar: %s",
					argv[0], config->current_bar->id);
			config->current_bar->position = strdup(argv[0]);
			return cmd_results_new(CMD_SUCCESS, NULL, NULL);
		}
	}

	error = cmd_results_new(CMD_INVALID, "position", "Invalid value %s", argv[0]);
	return error;
}
