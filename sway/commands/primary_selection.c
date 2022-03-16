#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "util.h"

struct cmd_results *cmd_primary_selection(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "primary_selection", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	bool primary_selection = parse_boolean(argv[0], true);

	if (config->reloading && config->primary_selection != primary_selection) {
		return cmd_results_new(CMD_FAILURE,
				"primary_selection can only be enabled/disabled at launch");
	}

	config->primary_selection = parse_boolean(argv[0], true);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
