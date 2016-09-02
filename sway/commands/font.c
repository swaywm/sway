#include <string.h>
#include "sway/border.h"
#include "sway/commands.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_font(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "font", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	char *font = join_args(argv, argc);
	free(config->font);
	if (strlen(font) > 6 && strncmp("pango:", font, 6) == 0) {
		config->font = strdup(font + 6);
		free(font);
	} else {
		config->font = font;
	}

	config->font_height = get_font_text_height(config->font);

	sway_log(L_DEBUG, "Settings font %s", config->font);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
