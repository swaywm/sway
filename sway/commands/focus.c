#include <strings.h>
#include <wlr/util/log.h>
#include "log.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/view.h"
#include "sway/commands.h"

static bool parse_movement_direction(const char *name, enum movement_direction *out) {
	if (strcasecmp(name, "left") == 0) {
		*out = MOVE_LEFT;
	} else if (strcasecmp(name, "right") == 0) {
		*out = MOVE_RIGHT;
	} else if (strcasecmp(name, "up") == 0) {
		*out = MOVE_UP;
	} else if (strcasecmp(name, "down") == 0) {
		*out = MOVE_DOWN;
	} else if (strcasecmp(name, "parent") == 0) {
		*out = MOVE_PARENT;
	} else if (strcasecmp(name, "child") == 0) {
		*out = MOVE_CHILD;
	} else if (strcasecmp(name, "next") == 0) {
		*out = MOVE_NEXT;
	} else if (strcasecmp(name, "prev") == 0) {
		*out = MOVE_PREV;
	} else {
		return false;
	}

	return true;
}

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
	if (con->type < C_WORKSPACE) {
		return cmd_results_new(CMD_FAILURE, "focus",
			"Command 'focus' cannot be used above the workspace level");
	}

	if (argc == 0) {
		sway_seat_set_focus(seat, con);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}

	// TODO mode_toggle
	enum movement_direction direction = 0;
	if (!parse_movement_direction(argv[0], &direction)) {
		return cmd_results_new(CMD_INVALID, "focus",
				"Expected 'focus <direction|parent|child|mode_toggle>' or 'focus output <direction|name>'");
	}

	swayc_t *next_focus = get_swayc_in_direction(con, seat, direction);
	if (next_focus) {
		sway_seat_set_focus(seat, next_focus);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
