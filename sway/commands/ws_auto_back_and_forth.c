#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "util.h"

struct cmd_results *cmd_ws_auto_back_and_forth(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "workspace_auto_back_and_forth", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	config->auto_back_and_forth = 
		parse_boolean(argv[0], config->auto_back_and_forth);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
