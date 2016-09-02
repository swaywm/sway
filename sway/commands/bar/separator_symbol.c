#include <string.h>
#include "commands.h"
#include "log.h"

struct cmd_results *bar_cmd_separator_symbol(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "separator_symbol", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "separator_symbol", "No bar defined.");
	}

	free(config->current_bar->separator_symbol);
	config->current_bar->separator_symbol = strdup(argv[0]);
	sway_log(L_DEBUG, "Settings separator_symbol '%s' for bar: %s", config->current_bar->separator_symbol, config->current_bar->id);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
