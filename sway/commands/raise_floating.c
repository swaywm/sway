#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "util.h"

struct cmd_results *cmd_raise_floating(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "raise_floating", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	config->raise_floating =
		parse_boolean(argv[0], config->raise_floating);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
