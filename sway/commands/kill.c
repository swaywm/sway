#include "sway/commands.h"
#include "sway/container.h"
#include "sway/focus.h"

struct cmd_results *cmd_kill(int argc, char **argv) {
	if (config->reading) return cmd_results_new(CMD_FAILURE, "kill", "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, "kill", "Can only be used when sway is running.");

	swayc_t *container = get_focused_container(&root_container);
	close_views(container);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
