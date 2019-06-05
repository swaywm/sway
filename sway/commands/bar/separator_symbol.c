#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *bar_cmd_separator_symbol(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "separator_symbol", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	free(config->current_bar->separator_symbol);
	config->current_bar->separator_symbol = strdup(argv[0]);
	sway_log(SWAY_DEBUG, "Settings separator_symbol '%s' for bar: %s",
			config->current_bar->separator_symbol, config->current_bar->id);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
