#include "sway/commands.h"

struct cmd_results *cmd_permit(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "permit", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	// TODO

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
