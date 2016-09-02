#include <stdio.h>
#include <string.h>
#include "commands.h"
#include "config.h"
#include "log.h"
#include "util.h"

struct cmd_results *cmd_bar(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "bar", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (config->reading && strcmp("{", argv[0]) != 0) {
		return cmd_results_new(CMD_INVALID, "bar",
				"Expected '{' at start of bar config definition.");
	}

	if (!config->reading) {
		if (argc > 1) {
			if (strcasecmp("mode", argv[0]) == 0) {
				return bar_cmd_mode(argc-1, argv + 1);
			}

			if (strcasecmp("hidden_state", argv[0]) == 0) {
				return bar_cmd_hidden_state(argc-1, argv + 1);
			}
		}

		return cmd_results_new(CMD_FAILURE, "bar", "Can only be used in config file.");
	}

	// Create new bar with default values
	struct bar_config *bar = default_bar_config();

	// set bar id
	int i;
	for (i = 0; i < config->bars->length; ++i) {
		if (bar == config->bars->items[i]) {
			const int len = 5 + numlen(i); // "bar-" + i + \0
			bar->id = malloc(len * sizeof(char));
			snprintf(bar->id, len, "bar-%d", i);
			break;
		}
	}

	// Set current bar
	config->current_bar = bar;
	sway_log(L_DEBUG, "Configuring bar %s", bar->id);
	return cmd_results_new(CMD_BLOCK_BAR, NULL, NULL);
}
