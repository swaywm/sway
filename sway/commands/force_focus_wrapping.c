#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *cmd_force_focus_wrapping(int argc, char **argv) {
	struct cmd_results *error =
		checkarg(argc, "force_focus_wrapping", EXPECTED_EQUAL_TO, 1);
	if (error) {
		return error;
	}

	if (strcasecmp(argv[0], "no") == 0) {
		config->focus_wrapping = WRAP_YES;
	} else if (strcasecmp(argv[0], "yes") == 0) {
		config->focus_wrapping = WRAP_FORCE;
	} else {
		return cmd_results_new(CMD_INVALID, "force_focus_wrapping",
				"Expected 'force_focus_wrapping yes|no'");
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
