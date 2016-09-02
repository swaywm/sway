#include <string.h>
#include "commands.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_exec(int argc, char **argv) {
	if (!config->active) return cmd_results_new(CMD_DEFER, "exec", NULL);
	if (config->reloading) {
		char *args = join_args(argv, argc);
		sway_log(L_DEBUG, "Ignoring 'exec %s' due to reload", args);
		free(args);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	return cmd_exec_always(argc, argv);
}

