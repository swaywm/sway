#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/view.h"
#include "sway/commands.h"

struct cmd_results *cmd_kill(int argc, char **argv) {
	struct sway_seat *seat = config->handler_context.seat;
	if (!seat) {
		seat = sway_input_manager_get_default_seat(input_manager);
	}

	// TODO context for arbitrary sway containers (when we get criteria
	// working) will make seat context not explicitly required
	if (!seat) {
		return cmd_results_new(CMD_FAILURE, NULL, "no seat context given");
	}

	struct sway_view *view = NULL;

	if (config->handler_context.current_container) {
		view = config->handler_context.current_container->sway_view;
	} else {
		view = seat->focus->sway_view;
	}

	if (view->iface.close) {
		view->iface.close(view);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
