#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *output_cmd_dpms(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "output", "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "output", "Missing dpms argument.");
	}

	if (strcmp(*argv, "on") == 0) {
		config->handler_context.output_config->dpms_state = DPMS_ON;
	} else if (strcmp(*argv, "off") == 0) {
		config->handler_context.output_config->dpms_state = DPMS_OFF;
	} else {
		return cmd_results_new(CMD_INVALID, "output",
				"Invalid dpms state, valid states are on/off.");
	}

	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;
	return NULL;
}
