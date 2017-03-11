#include <string.h>
#include <strings.h>
#include "sway/commands.h"

struct cmd_results *cmd_ws_auto_back_and_forth(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace_auto_back_and_forth", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (strcasecmp(argv[0], "yes") == 0) {
		config->auto_back_and_forth = true;
	} else if (strcasecmp(argv[0], "no") == 0) {
		config->auto_back_and_forth = false;
	} else {
		return cmd_results_new(CMD_INVALID, "workspace_auto_back_and_forth", "Expected 'workspace_auto_back_and_forth <yes|no>'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
