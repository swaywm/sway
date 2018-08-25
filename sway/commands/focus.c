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

/**
 * Get swayc in the direction of newly entered output.
 */
static struct sway_container *get_swayc_in_output_direction(
		struct sway_container *output, enum movement_direction dir,
		struct sway_seat *seat) {
	if (!output) {
		return NULL;
	}

	struct sway_container *ws = seat_get_focus_inactive(seat, output);
	if (ws->type != C_WORKSPACE) {
		ws = container_parent(ws, C_WORKSPACE);
	}

	if (ws == NULL) {
		wlr_log(WLR_ERROR, "got an output without a workspace");
		return NULL;
	}

	if (ws->children->length > 0) {
		switch (dir) {
		case MOVE_LEFT:
			if (ws->layout == L_HORIZ || ws->layout == L_TABBED) {
				// get most right child of new output
				return ws->children->items[ws->children->length-1];
			} else {
				return seat_get_focus_inactive(seat, ws);
			}
		case MOVE_RIGHT:
			if (ws->layout == L_HORIZ || ws->layout == L_TABBED) {
				// get most left child of new output
				return ws->children->items[0];
			} else {
				return seat_get_focus_inactive(seat, ws);
			}
		case MOVE_UP:
		case MOVE_DOWN: {
			struct sway_container *focused =
				seat_get_focus_inactive(seat, ws);
			if (focused && focused->parent) {
				struct sway_container *parent = focused->parent;
				if (parent->layout == L_VERT) {
					if (dir == MOVE_UP) {
						// get child furthest down on new output
						int idx = parent->children->length - 1;
						return parent->children->items[idx];
					} else if (dir == MOVE_DOWN) {
						// get child furthest up on new output
						return parent->children->items[0];
					}
				}
				return focused;
			}
			break;
		}
		default:
			break;
		}
	}

	return ws;
}

static struct sway_container *container_get_in_direction(
		struct sway_container *container, struct sway_seat *seat,
		enum movement_direction dir) {
	struct sway_container *parent = container->parent;

	if (dir == MOVE_CHILD) {
		return seat_get_focus_inactive(seat, container);
	}
	if (container->is_fullscreen) {
		if (dir == MOVE_PARENT) {
			return NULL;
		}
		container = container_parent(container, C_OUTPUT);
		parent = container->parent;
	} else {
		if (dir == MOVE_PARENT) {
			if (parent->type == C_OUTPUT || container_is_floating(container)) {
				return NULL;
			} else {
				return parent;
			}
		}
	}

	struct sway_container *wrap_candidate = NULL;
	while (true) {
		bool can_move = false;
		int desired;
		int idx = list_find(container->parent->children, container);
		if (idx == -1) {
			return NULL;
		}
		if (parent->type == C_ROOT) {
			enum wlr_direction wlr_dir = 0;
			if (!sway_assert(sway_dir_to_wlr(dir, &wlr_dir),
						"got invalid direction: %d", dir)) {
				return NULL;
			}
			int lx = container->x + container->width / 2;
			int ly = container->y + container->height / 2;
			struct wlr_output_layout *layout =
				root_container.sway_root->output_layout;
			struct wlr_output *wlr_adjacent =
				wlr_output_layout_adjacent_output(layout, wlr_dir,
					container->sway_output->wlr_output, lx, ly);
			struct sway_container *adjacent =
				output_from_wlr_output(wlr_adjacent);

			if (!adjacent || adjacent == container) {
				if (!wrap_candidate) {
					return NULL;
				}
				return seat_get_focus_inactive_view(seat, wrap_candidate);
			}
			struct sway_container *next =
				get_swayc_in_output_direction(adjacent, dir, seat);
			if (next == NULL) {
				return NULL;
			}
			struct sway_container *next_workspace = next;
			if (next_workspace->type != C_WORKSPACE) {
				next_workspace = container_parent(next_workspace, C_WORKSPACE);
			}
			sway_assert(next_workspace, "Next container has no workspace");
			if (next_workspace->sway_workspace->fullscreen) {
				return seat_get_focus_inactive(seat,
						next_workspace->sway_workspace->fullscreen);
			}
			if (next->children && next->children->length) {
				// TODO consider floating children as well
				return seat_get_focus_inactive_view(seat, next);
			} else {
				return next;
			}
		} else {
			if (dir == MOVE_LEFT || dir == MOVE_RIGHT) {
				if (parent->layout == L_HORIZ || parent->layout == L_TABBED) {
					can_move = true;
					desired = idx + (dir == MOVE_LEFT ? -1 : 1);
				}
			} else {
				if (parent->layout == L_VERT || parent->layout == L_STACKED) {
					can_move = true;
					desired = idx + (dir == MOVE_UP ? -1 : 1);
				}
			}
		}

		if (can_move) {
			// TODO handle floating
			if (desired < 0 || desired >= parent->children->length) {
				can_move = false;
				int len = parent->children->length;
				if (config->focus_wrapping != WRAP_NO && !wrap_candidate
						&& len > 1) {
					if (desired < 0) {
						wrap_candidate = parent->children->items[len-1];
					} else {
						wrap_candidate = parent->children->items[0];
					}
					if (config->focus_wrapping == WRAP_FORCE) {
						return seat_get_focus_inactive_view(seat,
								wrap_candidate);
					}
				}
			} else {
				struct sway_container *desired_con =
					parent->children->items[desired];
				wlr_log(WLR_DEBUG,
					"cont %d-%p dir %i sibling %d: %p", idx,
					container, dir, desired, desired_con);
				return seat_get_focus_inactive_view(seat, desired_con);
			}
		}

		if (!can_move) {
			container = parent;
			parent = parent->parent;
			if (!parent) {
				// wrapping is the last chance
				if (!wrap_candidate) {
					return NULL;
				}
				return seat_get_focus_inactive_view(seat, wrap_candidate);
			}
		}
	}
}

static struct cmd_results *focus_mode(struct sway_container *con,
		struct sway_seat *seat, bool floating) {
	struct sway_container *ws = con->type == C_WORKSPACE ?
		con : container_parent(con, C_WORKSPACE);

	// If the container is in a floating split container,
	// operate on the split container instead of the child.
	if (container_is_floating_or_child(con)) {
		while (con->parent->type != C_WORKSPACE) {
			con = con->parent;
		}
	}

	struct sway_container *new_focus = NULL;
	if (floating) {
		new_focus = seat_get_focus_inactive_floating(seat, ws);
	} else {
		new_focus = seat_get_focus_inactive_tiling(seat, ws);
	}
	if (new_focus) {
		seat_set_focus(seat, new_focus);
	} else {
		return cmd_results_new(CMD_FAILURE, "focus",
				"Failed to find a %s container in workspace",
				floating ? "floating" : "tiling");
	}
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
		return focus_mode(con, seat, !container_is_floating_or_child(con));
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
