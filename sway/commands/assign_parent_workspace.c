#include <string.h>
#include "sway/commands.h"
#include "sway/criteria.h"
#include "list.h"
#include "log.h"

struct cmd_results *cmd_assign_parent_workspace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "assign_parent_workspace", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	char *err_str = NULL;
	struct criteria *criteria = criteria_parse(argv[0], &err_str);
	if (!criteria) {
		error = cmd_results_new(CMD_INVALID, "%s", err_str);
		free(err_str);
		return error;
	}

	criteria->type = CT_ASSIGN_PARENT_WORKSPACE;

	// Check if it already exists
	if (criteria_already_exists(criteria)) {
		sway_log(SWAY_DEBUG, "assign_parent_workspace already exists: '%s'", criteria->raw);
		criteria_destroy(criteria);
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	list_add(config->criteria, criteria);
	sway_log(SWAY_DEBUG, "assign_parent_workspace: '%s' added", criteria->raw);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
