#include <string.h>
#include <strings.h>
#include "sway/commands.h"

struct cmd_results *cmd_force_focus_wrapping(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "force_focus_wrapping", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	config->force_focus_wrapping = !strcasecmp(argv[0], "yes");
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
