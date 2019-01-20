#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *output_cmd_enable(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}

	// The NOOP-1 output is a dummy output used when there's no outputs
	// connected. It should never be enabled.
	char *output_name = config->handler_context.output_config->name;
	if (strcasecmp(output_name, "NOOP-1") == 0) {
		return cmd_results_new(CMD_FAILURE,
				"Refusing to enable the no op output");
	}

	config->handler_context.output_config->enabled = 1;

	config->handler_context.leftovers.argc = argc;
	config->handler_context.leftovers.argv = argv;
	return NULL;
}

