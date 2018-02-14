#include <wlr/util/log.h>
#include "log.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/view.h"
#include "sway/commands.h"

struct cmd_results *cmd_focus(int argc, char **argv) {
	swayc_t *con = config->handler_context.current_container;
	struct sway_seat *seat = config->handler_context.seat;

	if (!sway_assert(seat, "'focus' command called without seat context")) {
		return cmd_results_new(CMD_FAILURE, "focus",
			"Command 'focus' called without seat context (this is a bug in sway)");
	}

	if (config->reading) {
		return cmd_results_new(CMD_FAILURE, "focus",
			"Command 'focus' cannot be used in the config file");
	}
	if (con == NULL) {
		wlr_log(L_DEBUG, "no container to focus");
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}

	if (argc == 0) {
		sway_seat_set_focus(seat, con);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
