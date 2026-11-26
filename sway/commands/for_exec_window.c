#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_for_exec_window(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "for_exec_window", EXPECTED_AT_LEAST, 2))) {
		return error;
	}
	if (!config->active || config->validating) {
		return cmd_results_new(CMD_DEFER, NULL);
	}
	if (config->reloading) {
		char *args = join_args(argv, argc);
		sway_log(SWAY_DEBUG, "Ignoring 'for_exec_window %s' due to reload", args);
		free(args);
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	char *cmdlist = strdup(argv[0]);
	strip_quotes(cmdlist);
	strip_whitespace(cmdlist);
	sway_log(SWAY_DEBUG, "for_exec_window: cmdlist='%s'", cmdlist);

	struct cmd_results *res = cmd_exec_process(argc - 1, argv + 1, cmdlist);
	free(cmdlist);
	return res;
}
