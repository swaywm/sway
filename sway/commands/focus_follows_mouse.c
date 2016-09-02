#include <string.h>
#include "commands.h"

struct cmd_results *cmd_focus_follows_mouse(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "focus_follows_mouse", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	config->focus_follows_mouse = !strcasecmp(argv[0], "yes");
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
