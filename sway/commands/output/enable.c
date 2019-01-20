#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *output_cmd_enable(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}

	config->handler_context.output_config->enabled = 1;

	config->handler_context.leftovers.argc = argc;
	config->handler_context.leftovers.argv = argv;
	return NULL;
}

