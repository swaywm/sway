#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"
#include "util.h"

struct cmd_results *cmd_force_focus_wrapping(int argc, char **argv) {
	sway_log(SWAY_INFO, "Warning: force_focus_wrapping is deprecated. "
		"Use focus_wrapping instead.");
	if (config->reading) {
		config_add_swaynag_warning("force_focus_wrapping is deprecated. "
			"Use focus_wrapping instead.");
	}

	struct cmd_results *error =
		checkarg(argc, "force_focus_wrapping", EXPECTED_EQUAL_TO, 1);
	if (error) {
		return error;
	}

	if (parse_boolean(argv[0], config->focus_wrapping == WRAP_FORCE)) {
		config->focus_wrapping = WRAP_FORCE;
	} else {
		config->focus_wrapping = WRAP_YES;
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
