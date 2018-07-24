#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "util.h"

struct cmd_results *cmd_force_focus_wrapping(int argc, char **argv) {
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

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
