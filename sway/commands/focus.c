#include <strings.h>
#include <wlr/util/log.h>
#include "log.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "stringop.h"

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
	} else {
		return false;
	}

	return true;
}

static struct cmd_results *focus_mode(struct sway_container *con,
		struct sway_seat *seat, bool floating) {
	struct sway_container *ws = con->type == C_WORKSPACE ?
		con : container_parent(con, C_WORKSPACE);
	struct sway_container *new_focus = ws;
	if (floating) {
		new_focus = ws->sway_workspace->floating;
		if (new_focus->children->length == 0) {
			return cmd_results_new(CMD_SUCCESS, NULL, NULL);
		}
	}
	seat_set_focus(seat, seat_get_active_child(seat, new_focus));
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *focus_output(struct sway_container *con,
		struct sway_seat *seat, int argc, char **argv) {
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "focus",
			"Expected 'focus output <direction|name>'");
	}
	char *identifier = join_args(argv, argc);
	struct sway_container *output = output_by_name(identifier);

	if (!output) {
		enum movement_direction direction;
		if (!parse_movement_direction(identifier, &direction) ||
				direction == MOVE_PARENT || direction == MOVE_CHILD) {
			free(identifier);
			return cmd_results_new(CMD_INVALID, "focus",
				"There is no output with that name");
		}
		struct sway_container *focus = seat_get_focus(seat);
		focus = container_parent(focus, C_OUTPUT);
		output = container_get_in_direction(focus, seat, direction);
	}

	free(identifier);
	if (output) {
		seat_set_focus(seat, seat_get_focus_inactive(seat, output));
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_focus(int argc, char **argv) {
	if (config->reading || !config->active) {
		return cmd_results_new(CMD_DEFER, NULL, NULL);
	}
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

	if (strcmp(argv[0], "floating") == 0) {
		return focus_mode(con, seat, true);
	} else if (strcmp(argv[0], "tiling") == 0) {
		return focus_mode(con, seat, false);
	} else if (strcmp(argv[0], "mode_toggle") == 0) {
		return focus_mode(con, seat, !container_is_floating(con));
	}

	if (strcmp(argv[0], "output") == 0) {
		argc--; argv++;
		return focus_output(con, seat, argc, argv);
	}

	enum movement_direction direction = 0;
	if (!parse_movement_direction(argv[0], &direction)) {
		return cmd_results_new(CMD_INVALID, "focus",
			"Expected 'focus <direction|parent|child|mode_toggle|floating|tiling>' "
			"or 'focus output <direction|name>'");
	}

	struct sway_container *next_focus = container_get_in_direction(
			con, seat, direction);
	if (next_focus) {
		seat_set_focus(seat, next_focus);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
