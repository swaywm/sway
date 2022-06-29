#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"
#include "stringop.h"
#include <pango/pangocairo.h>

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
		free(font);
	} else {
		config->pango_markup = false;
		config->font = font;
	}

	// Parse the font early so we can reject it if it's not valid for pango.
	// Also avoids re-parsing each time we render text.
	PangoFontDescription *font_description = pango_font_description_from_string(config->font);

	const char *family = pango_font_description_get_family(font_description);
	if (family == NULL) {
		pango_font_description_free(font_description);
		return cmd_results_new(CMD_FAILURE, "Invalid font family.");
	}

	const gint size = pango_font_description_get_size(font_description);
	if (size == 0) {
		pango_font_description_free(font_description);
		return cmd_results_new(CMD_FAILURE, "Invalid font size.");
	}

	if (config->font_description != NULL) {
		pango_font_description_free(config->font_description);
	}

	config->font_description = font_description;
	config_update_font_height();

	return cmd_results_new(CMD_SUCCESS, NULL);
}
