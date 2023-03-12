#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "util.h"

struct cmd_results *cmd_titlebar_separator(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "titlebar_separator", EXPECTED_EQUAL_TO, 1))) {
		return error;
	} else if(strcmp(argv[0], "disable") == 0) {
		config->titlebar_separator = false;
	} else if(strcmp(argv[0], "enable") == 0) {
		config->titlebar_separator = true;
	} else {
		return cmd_results_new(CMD_FAILURE,
				"Expected 'titlebar_separator enable|disable'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
