#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_exec(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = cmd_exec_validate(argc, argv))) {
		return error;
	}
	if (config->reloading) {
		char *args = join_args(argv, argc);
		sway_log(SWAY_DEBUG, "Ignoring 'exec %s' due to reload", args);
		free(args);
		return cmd_results_new(CMD_SUCCESS, NULL);
	}
	return cmd_exec_process(argc, argv);
}
