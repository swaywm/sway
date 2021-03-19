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
	char *font = join_args(argv, argc);
	free(config->current_bar->font);

	if (strncmp(font, "pango:", 6) == 0) {
		if (config->current_bar->pango_markup == PANGO_MARKUP_DEFAULT) {
			config->current_bar->pango_markup = true;
		}
		config->current_bar->font = strdup(font + 6);
	} else {
		if (config->current_bar->pango_markup == PANGO_MARKUP_DEFAULT) {
			config->current_bar->pango_markup = false;
		}
		config->current_bar->font = strdup(font);
	}

	free(font);
	sway_log(SWAY_DEBUG, "Settings font '%s' for bar: %s",
			config->current_bar->font, config->current_bar->id);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
