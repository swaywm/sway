#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "util.h"

struct cmd_results *cmd_primary_selection(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "primary_selection", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (parse_boolean(argv[0], true)) {
		config->primary_selection = PRIMARY_SELECTION_ENABLED;
	} else {
		config->primary_selection = PRIMARY_SELECTION_DISABLED;
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
