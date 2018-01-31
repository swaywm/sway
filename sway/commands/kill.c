#include <wlr/util/log.h>
#include "log.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/view.h"
#include "sway/commands.h"

struct cmd_results *cmd_kill(int argc, char **argv) {
	if (config->reading) {
		return cmd_results_new(CMD_FAILURE, "kill",
			"Command 'kill' cannot be used in the config file");
	}
	enum swayc_types type = config->handler_context.current_container->type;
	if (type != C_VIEW || type != C_CONTAINER) {
		return cmd_results_new(CMD_INVALID, NULL,
				"Can only kill views and containers with this command");
	}
	// TODO close arbitrary containers without a view
	struct sway_view *view =
		config->handler_context.current_container->sway_view;

	if (view) {
		view_close(view);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
