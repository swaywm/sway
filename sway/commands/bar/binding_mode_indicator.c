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
	config->current_bar->binding_mode_indicator =
		parse_boolean(argv[0], config->current_bar->binding_mode_indicator);
	if (config->current_bar->binding_mode_indicator) {
		sway_log(SWAY_DEBUG, "Enabling binding mode indicator on bar: %s",
				config->current_bar->id);
	} else {
		sway_log(SWAY_DEBUG, "Disabling binding mode indicator on bar: %s",
				config->current_bar->id);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
