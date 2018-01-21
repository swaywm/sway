#include <wlr/util/log.h>
#include "log.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/view.h"
#include "sway/commands.h"

struct cmd_results *cmd_kill(int argc, char **argv) {
	if (!sway_assert(config->handler_context.current_container,
				"cmd_kill called without container context")) {
		return cmd_results_new(CMD_INVALID, NULL,
				"cmd_kill called without container context "
				"(this is a bug in sway)");
	}
	// TODO close arbitrary containers without a view
	struct sway_view *view =
		config->handler_context.current_container->sway_view;

	if (view) {
		view_close(view);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
