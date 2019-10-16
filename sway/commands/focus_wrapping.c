#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "util.h"

struct cmd_results *cmd_focus_wrapping(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "focus_wrapping", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (strcasecmp(argv[0], "force") == 0) {
		config->focus_wrapping = WRAP_FORCE;
	} else if (strcasecmp(argv[0], "workspace") == 0) {
		config->focus_wrapping = WRAP_WORKSPACE;
	} else if (parse_boolean(argv[0], config->focus_wrapping == WRAP_YES)) {
		config->focus_wrapping = WRAP_YES;
	} else {
		config->focus_wrapping = WRAP_NO;
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
