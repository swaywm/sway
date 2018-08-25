#define _XOPEN_SOURCE 500
#include <string.h>
#include "sway/commands.h"
#include "sway/criteria.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_for_window(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "for_window", EXPECTED_EQUAL_TO, 2))) {
		return error;
	}

	// the arguments to cmd_for_window are specially preprocessed,
	// so that the command to be executed is provided as the final argument.
	char *cmdlist = strdup(argv[1]);
	if (!cmdlist) {
		return cmd_results_new(CMD_FAILURE, "for_window",
				"Unable to allocate a copy of the command");
	}

	char *err_str = NULL;
	struct criteria *criteria = criteria_parse(argv[0], &err_str);
	if (!criteria) {
		error = cmd_results_new(CMD_INVALID, "for_window", err_str);
		free(err_str);
		return error;
	}

	criteria->type = CT_COMMAND;
	criteria->cmdlist = cmdlist;

	list_add(config->criteria, criteria);
	wlr_log(WLR_DEBUG, "for_window: '%s' -> '%s' added", criteria->raw, criteria->cmdlist);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
