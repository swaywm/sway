#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "util.h"

struct cmd_results *cmd_focus_follows_mouse(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "focus_follows_mouse", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	config->focus_follows_mouse =
		parse_boolean(argv[0], config->focus_follows_mouse);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
