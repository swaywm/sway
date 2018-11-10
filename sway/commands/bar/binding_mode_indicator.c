#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "log.h"
#include "util.h"

struct cmd_results *bar_cmd_binding_mode_indicator(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc,
			"binding_mode_indicator", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (!config->current_bar) {
		return cmd_results_new(CMD_FAILURE,
				"binding_mode_indicator", "No bar defined.");
	}
	config->current_bar->binding_mode_indicator = 
		parse_boolean(argv[0], config->current_bar->binding_mode_indicator);
	if (config->current_bar->binding_mode_indicator) {
		wlr_log(WLR_DEBUG, "Enabling binding mode indicator on bar: %s",
				config->current_bar->id);
	} else {
		wlr_log(WLR_DEBUG, "Disabling binding mode indicator on bar: %s",
				config->current_bar->id);
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
