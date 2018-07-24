#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *output_cmd_mode(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "output", "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "output", "Missing mode argument.");
	}

	struct output_config *output = config->handler_context.output_config;

	char *end;
	output->width = strtol(*argv, &end, 10);
	if (*end) {
		// Format is 1234x4321
		if (*end != 'x') {
			return cmd_results_new(CMD_INVALID, "output",
				"Invalid mode width.");
		}
		++end;
		output->height = strtol(end, &end, 10);
		if (*end) {
			if (*end != '@') {
				return cmd_results_new(CMD_INVALID, "output",
					"Invalid mode height.");
			}
			++end;
			output->refresh_rate = strtof(end, &end);
			if (strcasecmp("Hz", end) != 0) {
				return cmd_results_new(CMD_INVALID, "output",
					"Invalid mode refresh rate.");
			}
		}
	} else {
		// Format is 1234 4321
		argc--; argv++;
		if (!argc) {
			return cmd_results_new(CMD_INVALID, "output",
				"Missing mode argument (height).");
		}
		output->height = strtol(*argv, &end, 10);
		if (*end) {
			return cmd_results_new(CMD_INVALID, "output",
				"Invalid mode height.");
		}
	}

	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;
	return NULL;
}

