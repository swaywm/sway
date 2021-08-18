#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_font(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "font", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	char *font = join_args(argv, argc);
	free(config->font);

	if (strncmp(font, "pango:", 6) == 0) {
		config->pango_markup = true;
		config->font = strdup(font + 6);
	} else {
		config->pango_markup = false;
		config->font = strdup(font);
	}

	free(font);
	config_update_font_height();
	return cmd_results_new(CMD_SUCCESS, NULL);
}
