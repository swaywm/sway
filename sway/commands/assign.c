#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <string.h>
#include "sway/commands.h"
#include "sway/criteria.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_assign(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "assign", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	// Create criteria
	char *err_str = NULL;
	struct criteria *criteria = criteria_parse(argv[0], &err_str);
	if (!criteria) {
		error = cmd_results_new(CMD_INVALID, "assign", err_str);
		free(err_str);
		return error;
	}

	++argv;
	int target_len = argc - 1;

	if (strncmp(*argv, "→", strlen("→")) == 0) {
		if (argc < 3) {
			free(criteria);
			return cmd_results_new(CMD_INVALID, "assign", "Missing workspace");
		}
		++argv;
		--target_len;
	}

	if (strcmp(*argv, "output") == 0) {
		criteria->type = CT_ASSIGN_OUTPUT;
		++argv;
		--target_len;
	} else {
		criteria->type = CT_ASSIGN_WORKSPACE;
	}

	criteria->target = join_args(argv, target_len);

	list_add(config->criteria, criteria);
	wlr_log(WLR_DEBUG, "assign: '%s' -> '%s' added", criteria->raw,
			criteria->target);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
