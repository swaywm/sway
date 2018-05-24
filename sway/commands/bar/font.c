#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *bar_cmd_font(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "font", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "font", "No bar defined.");
	}
	char *font = join_args(argv, argc);
	free(config->current_bar->font);
	config->current_bar->font = strdup(font);
	sway_log(L_DEBUG, "Settings font '%s' for bar: %s",
			config->current_bar->font, config->current_bar->id);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
