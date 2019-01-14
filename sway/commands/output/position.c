#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *output_cmd_position(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "Missing position argument.");
	}

	char *end;
	config->handler_context.output_config->x = strtol(*argv, &end, 10);
	if (*end) {
		// Format is 1234,4321
		if (*end != ',') {
			return cmd_results_new(CMD_INVALID, "Invalid position x.");
		}
		++end;
		config->handler_context.output_config->y = strtol(end, &end, 10);
		if (*end) {
			return cmd_results_new(CMD_INVALID, "Invalid position y.");
		}
	} else {
		// Format is 1234 4321 (legacy)
		argc--; argv++;
		if (!argc) {
			return cmd_results_new(CMD_INVALID, "Missing position argument (y).");
		}
		config->handler_context.output_config->y = strtol(*argv, &end, 10);
		if (*end) {
			return cmd_results_new(CMD_INVALID, "Invalid position y.");
		}
	}

	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;
	return NULL;
}

