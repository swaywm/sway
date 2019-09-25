#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *output_cmd_max_render_time(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "Missing max render time argument.");
	}

	int max_render_time;
	if (!strcmp(*argv, "off")) {
		max_render_time = 0;
	} else {
		char *end;
		max_render_time = strtol(*argv, &end, 10);
		if (*end || max_render_time <= 0) {
			return cmd_results_new(CMD_INVALID, "Invalid max render time.");
		}
	}
	config->handler_context.output_config->max_render_time = max_render_time;

	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;
	return NULL;
}
