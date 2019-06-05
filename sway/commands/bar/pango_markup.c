#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "log.h"
#include "util.h"

struct cmd_results *bar_cmd_pango_markup(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "pango_markup", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	config->current_bar->pango_markup =
		parse_boolean(argv[0], config->current_bar->pango_markup);
	if (config->current_bar->pango_markup) {
		sway_log(SWAY_DEBUG, "Enabling pango markup for bar: %s",
				config->current_bar->id);
	} else {
		sway_log(SWAY_DEBUG, "Disabling pango markup for bar: %s",
				config->current_bar->id);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
