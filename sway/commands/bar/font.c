#include <string.h>
#include "commands.h"
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
	if (strlen(font) > 6 && strncmp("pango:", font, 6) == 0) {
		config->current_bar->font = font;
	} else {
		config->current_bar->font = font;
	}

	sway_log(L_DEBUG, "Settings font '%s' for bar: %s", config->current_bar->font, config->current_bar->id);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
