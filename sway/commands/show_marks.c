#include <string.h>
#include <strings.h>
#include "sway/commands.h"

struct cmd_results *cmd_show_marks(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "show_marks", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	config->show_marks = !strcasecmp(argv[0], "on");
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
