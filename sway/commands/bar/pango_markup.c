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
	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE, "pango_markup", "No bar defined.");
	}
	config->current_bar->pango_markup 
		= parse_boolean(argv[0], config->current_bar->pango_markup);
	if (config->current_bar->pango_markup) {
		wlr_log(WLR_DEBUG, "Enabling pango markup for bar: %s",
				config->current_bar->id);
	} else {
		wlr_log(WLR_DEBUG, "Disabling pango markup for bar: %s",
				config->current_bar->id);
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
