#include <string.h>
#include "sway/commands.h"

struct cmd_results *cmd_smart_gaps(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "smart_gaps", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (strcasecmp(argv[0], "on") == 0) {
		config->smart_gaps = true;
	} else if (strcasecmp(argv[0], "off") == 0) {
		config->smart_gaps = false;
	} else {
		return cmd_results_new(CMD_INVALID, "smart_gaps", "Expected 'smart_gaps <on|off>'");
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
