#define _XOPEN_SOURCE 500
#include <string.h>
#include "sway/commands.h"
#include "sway/criteria.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_no_focus(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "no_focus", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	// add command to a criteria/command pair that is run against views when they appear.
	char *criteria = argv[0];

	struct criteria *crit = malloc(sizeof(struct criteria));
	if (!crit) {
		return cmd_results_new(CMD_FAILURE, "no_focus", "Unable to allocate criteria");
	}
	crit->crit_raw = strdup(criteria);
	crit->tokens = list_new(sizeof(struct crit_token *), 0);
	crit->cmdlist = NULL;
	char *err_str = extract_crit_tokens(crit->tokens, crit->crit_raw);

	if (err_str) {
		error = cmd_results_new(CMD_INVALID, "no_focus", err_str);
		free(err_str);
		free_criteria(crit);
	} else if (crit->tokens->length == 0) {
		error = cmd_results_new(CMD_INVALID, "no_focus", "Found no name/value pairs in criteria");
		free_criteria(crit);
	} else if (list_lsearchp(config->no_focus, criteria_cmp, crit, NULL) != -1) {
		sway_log(L_DEBUG, "no_focus: Duplicate, skipping.");
		free_criteria(crit);
	} else {
		sway_log(L_DEBUG, "no_focus: '%s' added", crit->crit_raw);
		list_add(config->no_focus, &crit);
	}
	return error ? error : cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
