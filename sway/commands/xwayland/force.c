#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *xwayland_cmd_force(int argc, char **argv) {
	if (config->reloading && config->xwayland != XWAYLAND_MODE_IMMEDIATE) {
		return cmd_results_new(CMD_FAILURE,
				"xwayland can only be enabled/disabled at launch");
	}
	config->xwayland = XWAYLAND_MODE_IMMEDIATE;

	config->handler_context.leftovers.argc = argc;
	config->handler_context.leftovers.argv = argv;
	return NULL;
}

