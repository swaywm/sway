#include <strings.h>
#include <wlr/util/log.h>
#include "log.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/tree/view.h"
#include "sway/commands.h"

static bool parse_movement_direction(const char *name,
		enum movement_direction *out) {
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
	struct sway_container *con = config->handler_context.current_container;
	struct sway_seat *seat = config->handler_context.seat;
	if (con->type < C_WORKSPACE) {
		return cmd_results_new(CMD_FAILURE, "focus",
			"Command 'focus' cannot be used above the workspace level");
	}

	if (argc == 0) {
		seat_set_focus(seat, con);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}

	// TODO mode_toggle
	enum movement_direction direction = 0;
	if (!parse_movement_direction(argv[0], &direction)) {
		return cmd_results_new(CMD_INVALID, "focus",
				"Expected 'focus <direction|parent|child|mode_toggle>' or 'focus output <direction|name>'");
	}

	struct sway_container *next_focus = container_get_in_direction(con, seat, direction);
	if (next_focus) {
		seat_set_focus(seat, next_focus);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
