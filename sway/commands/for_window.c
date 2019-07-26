#include <string.h>
#include "sway/commands.h"
#include "sway/criteria.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_for_window(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "for_window", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	char *err_str = NULL;
	struct criteria *criteria = criteria_parse(argv[0], &err_str);
	if (!criteria) {
		error = cmd_results_new(CMD_INVALID, err_str);
		free(err_str);
		return error;
	}

	criteria->type = CT_COMMAND;
	criteria->cmdlist = join_args(argv + 1, argc - 1);

	// Check if it already exists
	if (criteria_already_exists(criteria)) {
		sway_log(SWAY_DEBUG, "for_window already exists: '%s' -> '%s'",
				criteria->raw, criteria->cmdlist);
		criteria_destroy(criteria);
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	list_add(config->criteria, criteria);
	sway_log(SWAY_DEBUG, "for_window: '%s' -> '%s' added", criteria->raw, criteria->cmdlist);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
