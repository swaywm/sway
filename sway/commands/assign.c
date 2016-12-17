#include <stdio.h>
#include <string.h>
#include "sway/commands.h"
#include "sway/criteria.h"
#include "list.h"
#include "log.h"

struct cmd_results *cmd_assign(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "assign", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	char *criteria = *argv++;

	if (strncmp(*argv, "→", strlen("→")) == 0) {
		if (argc < 3) {
			return cmd_results_new(CMD_INVALID, "assign", "Missing workspace");
		}
		argv++;
	}

	char *movecmd = "move container to workspace ";
	int arglen = strlen(movecmd) + strlen(*argv) + 1;
	char *cmdlist = calloc(1, arglen);
	if (!cmdlist) {
		return cmd_results_new(CMD_FAILURE, "assign", "Unable to allocate command list");
	}
	snprintf(cmdlist, arglen, "%s%s", movecmd, *argv);

	struct criteria *crit = malloc(sizeof(struct criteria));
	if (!crit) {
		free(cmdlist);
		return cmd_results_new(CMD_FAILURE, "assign", "Unable to allocate criteria");
	}
	crit->crit_raw = strdup(criteria);
	crit->cmdlist = cmdlist;
	crit->tokens = create_list();
	char *err_str = extract_crit_tokens(crit->tokens, crit->crit_raw);

	if (err_str) {
		error = cmd_results_new(CMD_INVALID, "assign", err_str);
		free(err_str);
		free_criteria(crit);
	} else if (crit->tokens->length == 0) {
		error = cmd_results_new(CMD_INVALID, "assign", "Found no name/value pairs in criteria");
		free_criteria(crit);
	} else if (list_seq_find(config->criteria, criteria_cmp, crit) != -1) {
		sway_log(L_DEBUG, "assign: Duplicate, skipping.");
		free_criteria(crit);
	} else {
		sway_log(L_DEBUG, "assign: '%s' -> '%s' added", crit->crit_raw, crit->cmdlist);
		list_add(config->criteria, crit);
	}
	return error ? error : cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

