#include <string.h>
#include <strings.h>
#include "sway/commands.h"

struct cmd_results *cmd_seamless_mouse(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "seamless_mouse", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	config->seamless_mouse = (strcasecmp(argv[0], "on") == 0 || strcasecmp(argv[0], "yes") == 0);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
