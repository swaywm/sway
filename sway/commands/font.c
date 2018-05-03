#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_font(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "font", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	char *font = join_args(argv, argc);
	free(config->font);
	config->font = strdup(font);
	config_find_font_height(true);
	if (!config->reading) {
		arrange_root();
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
