#include <strings.h>
#include <wlr/util/log.h>
#include "log.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "stringop.h"
#include "util.h"

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

/**
 * Get node in the direction of newly entered output.
 */
static struct sway_node *get_node_in_output_direction(
		struct sway_output *output, enum movement_direction dir) {
	struct sway_seat *seat = config->handler_context.seat;
	struct sway_workspace *ws = output_get_active_workspace(output);
	if (ws->fullscreen) {
		return seat_get_focus_inactive(seat, &ws->fullscreen->node);
	}
	struct sway_container *container = NULL;

	if (ws->tiling->length > 0) {
		switch (dir) {
		case MOVE_LEFT:
			if (ws->layout == L_HORIZ || ws->layout == L_TABBED) {
				// get most right child of new output
				container = ws->tiling->items[ws->tiling->length-1];
			} else {
				container = seat_get_focus_inactive_tiling(seat, ws);
			}
			break;
		case MOVE_RIGHT:
			if (ws->layout == L_HORIZ || ws->layout == L_TABBED) {
				// get most left child of new output
				container = ws->tiling->items[0];
			} else {
				container = seat_get_focus_inactive_tiling(seat, ws);
			}
			break;
		case MOVE_UP:
			if (ws->layout == L_VERT || ws->layout == L_STACKED) {
				// get most bottom child of new output
				container = ws->tiling->items[ws->tiling->length-1];
			} else {
				container = seat_get_focus_inactive_tiling(seat, ws);
			}
			break;
		case MOVE_DOWN: {
			if (ws->layout == L_VERT || ws->layout == L_STACKED) {
				// get most top child of new output
				container = ws->tiling->items[0];
			} else {
				container = seat_get_focus_inactive_tiling(seat, ws);
			}
			break;
		}
		default:
			break;
		}
	}

	if (container) {
		container = seat_get_focus_inactive_view(seat, &container->node);
		return &container->node;
	}

	return &ws->node;
}

static struct sway_node *node_get_in_direction(struct sway_container *container,
		struct sway_seat *seat, enum movement_direction dir) {
	if (container->is_fullscreen) {
		if (dir == MOVE_PARENT) {
			return NULL;
		}
		// Fullscreen container with a direction - go straight to outputs
		struct sway_output *output = container->workspace->output;
		struct sway_output *new_output = output_get_in_direction(output, dir);
		if (!new_output) {
			return NULL;
		}
		return get_node_in_output_direction(new_output, dir);
	}
	if (dir == MOVE_PARENT) {
		return node_get_parent(&container->node);
	}

	struct sway_container *wrap_candidate = NULL;
	struct sway_container *current = container;
	while (current) {
		bool can_move = false;
		int desired;
		int idx = container_sibling_index(current);
		enum sway_container_layout parent_layout =
			container_parent_layout(current);
		list_t *siblings = container_get_siblings(current);

		if (dir == MOVE_LEFT || dir == MOVE_RIGHT) {
			if (parent_layout == L_HORIZ || parent_layout == L_TABBED) {
				can_move = true;
				desired = idx + (dir == MOVE_LEFT ? -1 : 1);
			}
		} else {
			if (parent_layout == L_VERT || parent_layout == L_STACKED) {
				can_move = true;
				desired = idx + (dir == MOVE_UP ? -1 : 1);
			}
		}

		if (can_move) {
			if (desired < 0 || desired >= siblings->length) {
				can_move = false;
				int len = siblings->length;
				if (config->focus_wrapping != WRAP_NO && !wrap_candidate
						&& len > 1) {
					if (desired < 0) {
						wrap_candidate = siblings->items[len-1];
					} else {
						wrap_candidate = siblings->items[0];
					}
					if (config->focus_wrapping == WRAP_FORCE) {
						struct sway_container *c = seat_get_focus_inactive_view(
								seat, &wrap_candidate->node);
						return &c->node;
					}
				}
			} else {
				struct sway_container *desired_con = siblings->items[desired];
				struct sway_container *c = seat_get_focus_inactive_view(
						seat, &desired_con->node);
				return &c->node;
			}
		}

		current = current->parent;
	}

	// Check a different output
	struct sway_output *output = container->workspace->output;
	struct sway_output *new_output = output_get_in_direction(output, dir);
	if (new_output) {
		return get_node_in_output_direction(new_output, dir);
	}
	return NULL;
}

static struct cmd_results *focus_mode(struct sway_workspace *ws,
		struct sway_seat *seat, bool floating) {
	struct sway_container *new_focus = NULL;
	if (floating) {
		new_focus = seat_get_focus_inactive_floating(seat, ws);
	} else {
		new_focus = seat_get_focus_inactive_tiling(seat, ws);
	}
	if (new_focus) {
		seat_set_focus_container(seat, new_focus);
		cursor_send_pointer_motion(seat->cursor, 0, true);
	} else {
		return cmd_results_new(CMD_FAILURE, "focus",
				"Failed to find a %s container in workspace",
				floating ? "floating" : "tiling");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *focus_output(struct sway_seat *seat,
		int argc, char **argv) {
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "focus",
			"Expected 'focus output <direction|name>'");
	}
	char *identifier = join_args(argv, argc);
	struct sway_output *output = output_by_name(identifier);

	if (!output) {
		enum movement_direction direction;
		if (!parse_movement_direction(identifier, &direction) ||
				direction == MOVE_PARENT || direction == MOVE_CHILD) {
			free(identifier);
			return cmd_results_new(CMD_INVALID, "focus",
				"There is no output with that name");
		}
		struct sway_workspace *ws = seat_get_focused_workspace(seat);
		output = output_get_in_direction(ws->output, direction);
	}

	free(identifier);
	if (output) {
		seat_set_focus(seat, seat_get_focus_inactive(seat, &output->node));
		cursor_send_pointer_motion(seat->cursor, 0, true);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_focus(int argc, char **argv) {
	if (config->reading || !config->active) {
		return cmd_results_new(CMD_DEFER, NULL, NULL);
	}
	struct sway_node *node = config->handler_context.node;
	struct sway_container *container = config->handler_context.container;
	struct sway_workspace *workspace = config->handler_context.workspace;
	struct sway_seat *seat = config->handler_context.seat;
	if (node->type < N_WORKSPACE) {
		return cmd_results_new(CMD_FAILURE, "focus",
			"Command 'focus' cannot be used above the workspace level");
	}

	if (argc == 0 && container) {
		seat_set_focus_container(seat, container);
		cursor_send_pointer_motion(seat->cursor, 0, true);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}

	if (strcmp(argv[0], "floating") == 0) {
		return focus_mode(workspace, seat, true);
	} else if (strcmp(argv[0], "tiling") == 0) {
		return focus_mode(workspace, seat, false);
	} else if (strcmp(argv[0], "mode_toggle") == 0) {
		bool floating = container && container_is_floating_or_child(container);
		return focus_mode(workspace, seat, !floating);
	}

	if (strcmp(argv[0], "output") == 0) {
		argc--; argv++;
		return focus_output(seat, argc, argv);
	}

	enum movement_direction direction = 0;
	if (!parse_movement_direction(argv[0], &direction)) {
		return cmd_results_new(CMD_INVALID, "focus",
			"Expected 'focus <direction|parent|child|mode_toggle|floating|tiling>' "
			"or 'focus output <direction|name>'");
	}

	if (direction == MOVE_CHILD) {
		struct sway_node *focus = seat_get_active_child(seat, node);
		if (focus) {
			seat_set_focus(seat, focus);
			cursor_send_pointer_motion(seat->cursor, 0, true);
		}
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}

	if (node->type == N_WORKSPACE) {
		if (direction == MOVE_PARENT) {
			return cmd_results_new(CMD_SUCCESS, NULL, NULL);
		}

		// Jump to the next output
		struct sway_output *new_output =
			output_get_in_direction(workspace->output, direction);
		if (!new_output) {
			return cmd_results_new(CMD_SUCCESS, NULL, NULL);
		}

		struct sway_node *node =
			get_node_in_output_direction(new_output, direction);
		seat_set_focus(seat, node);
		cursor_send_pointer_motion(seat->cursor, 0, true);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}

	struct sway_node *next_focus =
		node_get_in_direction(container, seat, direction);
	if (next_focus) {
		seat_set_focus(seat, next_focus);
		cursor_send_pointer_motion(seat->cursor, 0, true);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
