#include <strings.h>
#include <wlr/types/wlr_output_layout.h>
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

static bool parse_direction(const char *name,
		enum wlr_direction *out) {
	if (strcasecmp(name, "left") == 0) {
		*out = WLR_DIRECTION_LEFT;
	} else if (strcasecmp(name, "right") == 0) {
		*out = WLR_DIRECTION_RIGHT;
	} else if (strcasecmp(name, "up") == 0) {
		*out = WLR_DIRECTION_UP;
	} else if (strcasecmp(name, "down") == 0) {
		*out = WLR_DIRECTION_DOWN;
	} else {
		return false;
	}

	return true;
}

/**
 * Get node in the direction of newly entered output.
 */
static struct sway_node *get_node_in_output_direction(
		struct sway_output *output, enum wlr_direction dir) {
	struct sway_seat *seat = config->handler_context.seat;
	struct sway_workspace *ws = output_get_active_workspace(output);
	if (ws->fullscreen) {
		return seat_get_focus_inactive(seat, &ws->fullscreen->node);
	}
	struct sway_container *container = NULL;

	if (ws->tiling->length > 0) {
		switch (dir) {
		case WLR_DIRECTION_LEFT:
			if (ws->layout == L_HORIZ || ws->layout == L_TABBED) {
				// get most right child of new output
				container = ws->tiling->items[ws->tiling->length-1];
			} else {
				container = seat_get_focus_inactive_tiling(seat, ws);
			}
			break;
		case WLR_DIRECTION_RIGHT:
			if (ws->layout == L_HORIZ || ws->layout == L_TABBED) {
				// get most left child of new output
				container = ws->tiling->items[0];
			} else {
				container = seat_get_focus_inactive_tiling(seat, ws);
			}
			break;
		case WLR_DIRECTION_UP:
			if (ws->layout == L_VERT || ws->layout == L_STACKED) {
				// get most bottom child of new output
				container = ws->tiling->items[ws->tiling->length-1];
			} else {
				container = seat_get_focus_inactive_tiling(seat, ws);
			}
			break;
		case WLR_DIRECTION_DOWN:
			if (ws->layout == L_VERT || ws->layout == L_STACKED) {
				// get most top child of new output
				container = ws->tiling->items[0];
			} else {
				container = seat_get_focus_inactive_tiling(seat, ws);
			}
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
		struct sway_seat *seat, enum wlr_direction dir) {
	struct sway_container *wrap_candidate = NULL;
	struct sway_container *current = container;
	while (current) {
		if (current->fullscreen_mode == FULLSCREEN_WORKSPACE) {
			// Fullscreen container with a direction - go straight to outputs
			struct sway_output *output = current->workspace->output;
			struct sway_output *new_output =
				output_get_in_direction(output, dir);
			if (!new_output) {
				return NULL;
			}
			return get_node_in_output_direction(new_output, dir);
		}
		if (current->fullscreen_mode == FULLSCREEN_GLOBAL) {
			return NULL;
		}

		bool can_move = false;
		int desired;
		int idx = container_sibling_index(current);
		enum sway_container_layout parent_layout =
			container_parent_layout(current);
		list_t *siblings = container_get_siblings(current);

		if (dir == WLR_DIRECTION_LEFT || dir == WLR_DIRECTION_RIGHT) {
			if (parent_layout == L_HORIZ || parent_layout == L_TABBED) {
				can_move = true;
				desired = idx + (dir == WLR_DIRECTION_LEFT ? -1 : 1);
			}
		} else {
			if (parent_layout == L_VERT || parent_layout == L_STACKED) {
				can_move = true;
				desired = idx + (dir == WLR_DIRECTION_UP ? -1 : 1);
			}
		}

		if (can_move) {
			if (desired < 0 || desired >= siblings->length) {
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

	// If there is a wrap candidate, return its focus inactive view
	if (wrap_candidate) {
		struct sway_container *wrap_inactive = seat_get_focus_inactive_view(
				seat, &wrap_candidate->node);
		return &wrap_inactive->node;
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
		seat_consider_warp_to_focus(seat);
	} else {
		return cmd_results_new(CMD_FAILURE,
				"Failed to find a %s container in workspace",
				floating ? "floating" : "tiling");
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

static struct cmd_results *focus_output(struct sway_seat *seat,
		int argc, char **argv) {
	if (!argc) {
		return cmd_results_new(CMD_INVALID,
			"Expected 'focus output <direction|name>'");
	}
	char *identifier = join_args(argv, argc);
	struct sway_output *output = output_by_name_or_id(identifier);

	if (!output) {
		enum wlr_direction direction;
		if (!parse_direction(identifier, &direction)) {
			free(identifier);
			return cmd_results_new(CMD_INVALID,
				"There is no output with that name");
		}
		struct sway_workspace *ws = seat_get_focused_workspace(seat);
		output = output_get_in_direction(ws->output, direction);

		if (!output) {
			int center_lx = ws->output->lx + ws->output->width / 2;
			int center_ly = ws->output->ly + ws->output->height / 2;
			struct wlr_output *target = wlr_output_layout_farthest_output(
					root->output_layout, opposite_direction(direction),
					ws->output->wlr_output, center_lx, center_ly);
			if (target) {
				output = output_from_wlr_output(target);
			}
		}
	}

	free(identifier);
	if (output) {
		seat_set_focus(seat, seat_get_focus_inactive(seat, &output->node));
		seat_consider_warp_to_focus(seat);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

static struct cmd_results *focus_parent(void) {
	struct sway_seat *seat = config->handler_context.seat;
	struct sway_container *con = config->handler_context.container;
	if (!con || con->fullscreen_mode) {
		return cmd_results_new(CMD_SUCCESS, NULL);
	}
	struct sway_node *parent = node_get_parent(&con->node);
	if (parent) {
		seat_set_focus(seat, parent);
		seat_consider_warp_to_focus(seat);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

static struct cmd_results *focus_child(void) {
	struct sway_seat *seat = config->handler_context.seat;
	struct sway_node *node = config->handler_context.node;
	struct sway_node *focus = seat_get_active_tiling_child(seat, node);
	if (focus) {
		seat_set_focus(seat, focus);
		seat_consider_warp_to_focus(seat);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_focus(int argc, char **argv) {
	if (config->reading || !config->active) {
		return cmd_results_new(CMD_DEFER, NULL);
	}
	if (!root->outputs->length) {
		return cmd_results_new(CMD_INVALID,
				"Can't run this command while there's no outputs connected.");
	}
	struct sway_node *node = config->handler_context.node;
	struct sway_container *container = config->handler_context.container;
	struct sway_workspace *workspace = config->handler_context.workspace;
	struct sway_seat *seat = config->handler_context.seat;
	if (node->type < N_WORKSPACE) {
		return cmd_results_new(CMD_FAILURE,
			"Command 'focus' cannot be used above the workspace level");
	}

	if (argc == 0 && container) {
		if (container_is_scratchpad_hidden(container)) {
			root_scratchpad_show(container);
		}
		seat_set_focus_container(seat, container);
		seat_consider_warp_to_focus(seat);
		return cmd_results_new(CMD_SUCCESS, NULL);
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

	if (strcasecmp(argv[0], "parent") == 0) {
		return focus_parent();
	}
	if (strcasecmp(argv[0], "child") == 0) {
		return focus_child();
	}

	enum wlr_direction direction = 0;
	if (!parse_direction(argv[0], &direction)) {
		return cmd_results_new(CMD_INVALID,
			"Expected 'focus <direction|parent|child|mode_toggle|floating|tiling>' "
			"or 'focus output <direction|name>'");
	}

	if (node->type == N_WORKSPACE) {
		// Jump to the next output
		struct sway_output *new_output =
			output_get_in_direction(workspace->output, direction);
		if (!new_output) {
			return cmd_results_new(CMD_SUCCESS, NULL);
		}

		struct sway_node *node =
			get_node_in_output_direction(new_output, direction);
		seat_set_focus(seat, node);
		seat_consider_warp_to_focus(seat);
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	struct sway_node *next_focus =
		node_get_in_direction(container, seat, direction);
	if (next_focus) {
		seat_set_focus(seat, next_focus);
		seat_consider_warp_to_focus(seat);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
