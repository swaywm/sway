#define _XOPEN_SOURCE 500
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
	// add command to a criteria/command pair that is run against views when they appear.
	char *criteria = argv[0], *cmdlist = join_args(argv + 1, argc - 1);

	struct criteria *crit = malloc(sizeof(struct criteria));
	if (!crit) {
		return cmd_results_new(CMD_FAILURE, "for_window", "Unable to allocate criteria");
	}
	crit->crit_raw = strdup(criteria);
	crit->cmdlist = cmdlist;
	crit->tokens = list_new(sizeof(struct crit_token *), 0);
	char *err_str = extract_crit_tokens(crit->tokens, crit->crit_raw);

	if (err_str) {
		error = cmd_results_new(CMD_INVALID, "for_window", err_str);
		free(err_str);
		free_criteria(crit);
	} else if (crit->tokens->length == 0) {
		error = cmd_results_new(CMD_INVALID, "for_window", "Found no name/value pairs in criteria");
		free_criteria(crit);
	} else if (list_lsearchp(config->criteria, criteria_cmp, crit, NULL) != -1) {
		sway_log(L_DEBUG, "for_window: Duplicate, skipping.");
		free_criteria(crit);
	} else {
		sway_log(L_DEBUG, "for_window: '%s' -> '%s' added", crit->crit_raw, crit->cmdlist);
		list_add(config->criteria, &crit);
	}
	return error ? error : cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
