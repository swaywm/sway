#define _XOPEN_SOURCE 500
#include <string.h>
#include "sway/commands.h"
#include "sway/criteria.h"
#include "list.h"
#include "log.h"

struct cmd_results *cmd_no_focus(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "no_focus", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	char *err_str = NULL;
	struct criteria *criteria = criteria_parse(argv[0], &err_str);
	if (!criteria) {
		error = cmd_results_new(CMD_INVALID, "no_focus", err_str);
		free(err_str);
		return error;
	}

	criteria->type = CT_NO_FOCUS;
	list_add(config->criteria, criteria);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
