#include "sway/commands.h"
#include "sway/config.h"
#include "util.h"

struct cmd_results *output_cmd_dpms(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "Missing dpms argument.");
	}

	if (parse_boolean(argv[0], true)) {
		config->handler_context.output_config->dpms_state = DPMS_ON;
	} else {
		config->handler_context.output_config->dpms_state = DPMS_OFF;
	}

	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;
	return NULL;
}
