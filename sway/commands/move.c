#define _XOPEN_SOURCE 500
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/log.h>
#include "sway/commands.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/workspace.h"
#include "stringop.h"
#include "list.h"
#include "log.h"

static const char *expected_syntax =
	"Expected 'move <left|right|up|down> <[px] px>' or "
	"'move [--no-auto-back-and-forth] <container|window> [to] workspace <name>' or "
	"'move <container|window|workspace> [to] output <name|direction>' or "
	"'move <container|window> [to] mark <mark>'";

static struct sway_container *output_in_direction(const char *direction,
		struct wlr_output *reference, int ref_lx, int ref_ly) {
	struct {
		char *name;
		enum wlr_direction direction;
	} names[] = {
		{ "up", WLR_DIRECTION_UP },
		{ "down", WLR_DIRECTION_DOWN },
		{ "left", WLR_DIRECTION_LEFT },
		{ "right", WLR_DIRECTION_RIGHT },
	};
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
		if (strcasecmp(names[i].name, direction) == 0) {
			struct wlr_output *adjacent = wlr_output_layout_adjacent_output(
					root_container.sway_root->output_layout,
					names[i].direction, reference, ref_lx, ref_ly);
			if (adjacent) {
				struct sway_output *sway_output = adjacent->data;
				return sway_output->swayc;
			}
			break;
		}
	}
	return output_by_name(direction);
}

static void container_move_to(struct sway_container *container,
		struct sway_container *destination) {
	if (!sway_assert(container->type == C_CONTAINER ||
				container->type == C_VIEW, "Expected a container or view")) {
		return;
	}
	if (container == destination
			|| container_has_ancestor(container, destination)) {
		return;
	}
	struct sway_container *old_parent = NULL;
	struct sway_container *new_parent = NULL;
	if (container_is_floating(container)) {
		// Resolve destination into a workspace
		struct sway_container *new_ws = NULL;
		if (destination->type == C_OUTPUT) {
			new_ws = output_get_active_workspace(destination->sway_output);
		} else if (destination->type == C_WORKSPACE) {
			new_ws = destination;
		} else {
			new_ws = container_parent(destination, C_WORKSPACE);
		}
		if (!new_ws) {
			// This can happen if the user has run "move container to mark foo",
			// where mark foo is on a hidden scratchpad container.
			return;
		}
		struct sway_container *old_output =
			container_parent(container, C_OUTPUT);
		old_parent = container_remove_child(container);
		workspace_add_floating(new_ws, container);
		container_handle_fullscreen_reparent(container, old_parent);
		// If changing output, center it within the workspace
		if (old_output != new_ws->parent && !container->is_fullscreen) {
			container_floating_move_to_center(container);
		}
	} else {
		old_parent = container_remove_child(container);
		container->width = container->height = 0;
		container->saved_width = container->saved_height = 0;

		if (destination->type == C_VIEW) {
			new_parent = container_add_sibling(destination, container);
		} else {
			new_parent = destination;
			container_add_child(destination, container);
		}
	}

	if (container->type == C_VIEW) {
		ipc_event_window(container, "move");
	}
	container_notify_subtree_changed(old_parent);
	container_notify_subtree_changed(new_parent);

	// If view was moved to a fullscreen workspace, refocus the fullscreen view
	struct sway_container *new_workspace = container;
	if (new_workspace->type != C_WORKSPACE) {
		new_workspace = container_parent(new_workspace, C_WORKSPACE);
	}
	if (new_workspace->sway_workspace->fullscreen) {
		struct sway_seat *seat;
		struct sway_container *focus, *focus_ws;
		wl_list_for_each(seat, &input_manager->seats, link) {
			focus = seat_get_focus(seat);
			focus_ws = focus;
			if (focus_ws->type != C_WORKSPACE) {
				focus_ws = container_parent(focus_ws, C_WORKSPACE);
			}
			if (focus_ws == new_workspace) {
				struct sway_container *new_focus = seat_get_focus_inactive(seat,
						new_workspace->sway_workspace->fullscreen);
				seat_set_focus(seat, new_focus);
			}
		}
	}
	// Update workspace urgent state
	struct sway_container *old_workspace = old_parent;
	if (old_workspace->type != C_WORKSPACE) {
		old_workspace = container_parent(old_workspace, C_WORKSPACE);
	}
	if (new_workspace != old_workspace) {
		workspace_detect_urgent(new_workspace);
		if (old_workspace) {
			workspace_detect_urgent(old_workspace);
		}
	}
}

static bool is_parallel(enum sway_container_layout layout,
		enum movement_direction dir) {
	switch (layout) {
	case L_TABBED:
	case L_HORIZ:
		return dir == MOVE_LEFT || dir == MOVE_RIGHT;
	case L_STACKED:
	case L_VERT:
		return dir == MOVE_UP || dir == MOVE_DOWN;
	default:
		return false;
	}
}

static enum movement_direction invert_movement(enum movement_direction dir) {
	switch (dir) {
	case MOVE_LEFT:
		return MOVE_RIGHT;
	case MOVE_RIGHT:
		return MOVE_LEFT;
	case MOVE_UP:
		return MOVE_DOWN;
	case MOVE_DOWN:
		return MOVE_UP;
	default:
		sway_assert(0, "This function expects left|right|up|down");
		return MOVE_LEFT;
	}
}

static int move_offs(enum movement_direction move_dir) {
	return move_dir == MOVE_LEFT || move_dir == MOVE_UP ? -1 : 1;
}

/* Gets the index of the most extreme member based on the movement offset */
static int container_limit(struct sway_container *container,
		enum movement_direction move_dir) {
	return move_offs(move_dir) < 0 ? 0 : container->children->length;
}

/* Takes one child, sets it aside, wraps the rest of the children in a new
 * container, switches the layout of the workspace, and drops the child back in.
 * In other words, rejigger it. */
static void workspace_rejigger(struct sway_container *ws,
		struct sway_container *child, enum movement_direction move_dir) {
	struct sway_container *original_parent = child->parent;
	struct sway_container *new_parent =
		container_split(ws, ws->layout);

	container_remove_child(child);
	for (int i = 0; i < ws->children->length; ++i) {
		struct sway_container *_child = ws->children->items[i];
		container_move_to(new_parent, _child);
	}

	int index = move_offs(move_dir);
	container_insert_child(ws, child, index < 0 ? 0 : 1);
	ws->layout =
		move_dir == MOVE_LEFT || move_dir == MOVE_RIGHT ? L_HORIZ : L_VERT;

	container_flatten(ws);
	container_reap_empty(original_parent);
	container_create_notify(new_parent);
}

static void move_out_of_tabs_stacks(struct sway_container *container,
		struct sway_container *current, enum movement_direction move_dir,
		int offs) {
	if (container->parent == current->parent
			&& current->parent->children->length == 1) {
		wlr_log(WLR_DEBUG, "Changing layout of %zd", current->parent->id);
		current->parent->layout = move_dir ==
			MOVE_LEFT || move_dir == MOVE_RIGHT ? L_HORIZ : L_VERT;
		return;
	}

	wlr_log(WLR_DEBUG, "Moving out of tab/stack into a split");
	bool is_workspace = current->parent->type == C_WORKSPACE;
	struct sway_container *new_parent = container_split(current->parent,
		move_dir == MOVE_LEFT || move_dir == MOVE_RIGHT ? L_HORIZ : L_VERT);
	if (is_workspace) {
		container_insert_child(new_parent->parent, container, offs < 0 ? 0 : 1);
	} else {
		container_insert_child(new_parent, container, offs < 0 ? 0 : 1);
		container_reap_empty(new_parent->parent);
		container_flatten(new_parent->parent);
	}
	container_create_notify(new_parent);
	container_notify_subtree_changed(new_parent);
}

static void container_move(struct sway_container *container,
		enum movement_direction move_dir, int move_amt) {
	if (!sway_assert(
				container->type != C_CONTAINER || container->type != C_VIEW,
				"Can only move containers and views")) {
		return;
	}
	int offs = move_offs(move_dir);

	struct sway_container *sibling = NULL;
	struct sway_container *current = container;
	struct sway_container *parent = current->parent;
	struct sway_container *top = &root_container;

	// If moving a fullscreen view, only consider outputs
	if (container->is_fullscreen) {
		current = container_parent(container, C_OUTPUT);
	} else if (container_is_fullscreen_or_child(container) ||
			container_is_floating_or_child(container)) {
		// If we've fullscreened a split container, only allow the child to move
		// around within the fullscreen parent.
		// Same with floating a split container.
		struct sway_container *ws = container_parent(container, C_WORKSPACE);
		top = ws->sway_workspace->fullscreen;
	}

	struct sway_container *new_parent = container_flatten(parent);
	if (new_parent != parent) {
		// Special case: we were the last one in this container, so leave
		return;
	}

	while (!sibling) {
		if (current == top) {
			return;
		}

		parent = current->parent;
		wlr_log(WLR_DEBUG, "Visiting %p %s '%s'", current,
				container_type_to_str(current->type), current->name);

		int index = container_sibling_index(current);

		switch (current->type) {
		case C_OUTPUT: {
			enum wlr_direction wlr_dir = 0;
			if (!sway_assert(sway_dir_to_wlr(move_dir, &wlr_dir),
						"got invalid direction: %d", move_dir)) {
				return;
			}
			double ref_lx = current->x + current->width / 2;
			double ref_ly = current->y + current->height / 2;
			struct wlr_output *next = wlr_output_layout_adjacent_output(
				root_container.sway_root->output_layout, wlr_dir,
				current->sway_output->wlr_output, ref_lx, ref_ly);
			if (!next) {
				wlr_log(WLR_DEBUG, "Hit edge of output, nowhere else to go");
				return;
			}
			struct sway_output *next_output = next->data;
			current = next_output->swayc;
			wlr_log(WLR_DEBUG, "Selected next output (%s)", current->name);
			// Select workspace and get outta here
			current = seat_get_focus_inactive(
					config->handler_context.seat, current);
			if (current->type != C_WORKSPACE) {
				current = container_parent(current, C_WORKSPACE);
			}
			sibling = current;
			break;
		}
		case C_WORKSPACE:
			if (!is_parallel(current->layout, move_dir)) {
				if (current->children->length >= 2) {
					wlr_log(WLR_DEBUG, "Rejiggering the workspace (%d kiddos)",
							current->children->length);
					workspace_rejigger(current, container, move_dir);
					return;
				} else {
					wlr_log(WLR_DEBUG, "Selecting output");
					current = current->parent;
				}
			} else if (current->layout == L_TABBED
					|| current->layout == L_STACKED) {
				wlr_log(WLR_DEBUG, "Rejiggering out of tabs/stacks");
				workspace_rejigger(current, container, move_dir);
			} else {
				wlr_log(WLR_DEBUG, "Selecting output");
				current = current->parent;
			}
			break;
		case C_CONTAINER:
		case C_VIEW:
			if (is_parallel(parent->layout, move_dir)) {
				if ((index == parent->children->length - 1 && offs > 0)
						|| (index == 0 && offs < 0)) {
					if (current->parent == container->parent) {
						if (!parent->is_fullscreen &&
								(parent->layout == L_TABBED ||
								 parent->layout == L_STACKED)) {
							move_out_of_tabs_stacks(container, current,
									move_dir, offs);
							return;
						} else {
							wlr_log(WLR_DEBUG, "Hit limit, selecting parent");
							current = current->parent;
						}
					} else {
						wlr_log(WLR_DEBUG, "Hit limit, "
								"promoting descendant to sibling");
						// Special case
						container_insert_child(current->parent, container,
								index + (offs < 0 ? 0 : 1));
						container->width = container->height = 0;
						return;
					}
				} else {
					sibling = parent->children->items[index + offs];
					wlr_log(WLR_DEBUG, "Selecting sibling id:%zd", sibling->id);
				}
			} else if (!parent->is_fullscreen && (parent->layout == L_TABBED ||
						parent->layout == L_STACKED)) {
				move_out_of_tabs_stacks(container, current, move_dir, offs);
				return;
			} else {
				wlr_log(WLR_DEBUG, "Moving up to find a parallel container");
				current = current->parent;
			}
			break;
		default:
			sway_assert(0, "Not expecting to see container of type %s here",
					container_type_to_str(current->type));
			return;
		}
	}

	// Part two: move stuff around
	int index = container_sibling_index(container);
	struct sway_container *old_parent = container->parent;

	while (sibling) {
		switch (sibling->type) {
		case C_VIEW:
			if (sibling->parent == container->parent) {
				wlr_log(WLR_DEBUG, "Swapping siblings");
				sibling->parent->children->items[index + offs] = container;
				sibling->parent->children->items[index] = sibling;
			} else {
				wlr_log(WLR_DEBUG, "Promoting to sibling of cousin");
				container_insert_child(sibling->parent, container,
						container_sibling_index(sibling) + (offs > 0 ? 0 : 1));
				container->width = container->height = 0;
			}
			sibling = NULL;
			break;
		case C_WORKSPACE: // Note: only in the case of moving between outputs
		case C_CONTAINER:
			if (is_parallel(sibling->layout, move_dir)) {
				int limit = container_limit(sibling, invert_movement(move_dir));
				wlr_log(WLR_DEBUG, "limit: %d", limit);
				wlr_log(WLR_DEBUG,
						"Reparenting container (parallel) to index %d "
						"(move dir: %d)", limit, move_dir);
				container_insert_child(sibling, container, limit);
				container->width = container->height = 0;
				sibling = NULL;
			} else {
				wlr_log(WLR_DEBUG, "Reparenting container (perpendicular)");
				struct sway_container *focus_inactive = seat_get_focus_inactive(
						config->handler_context.seat, sibling);
				if (focus_inactive && focus_inactive != sibling) {
					while (focus_inactive->parent != sibling) {
						focus_inactive = focus_inactive->parent;
					}
					wlr_log(WLR_DEBUG, "Focus inactive: id:%zd",
							focus_inactive->id);
					sibling = focus_inactive;
					continue;
				} else if (sibling->children->length) {
					wlr_log(WLR_DEBUG, "No focus-inactive, adding arbitrarily");
					container_remove_child(container);
					container_add_sibling(sibling->children->items[0], container);
				} else {
					wlr_log(WLR_DEBUG, "No kiddos, adding container alone");
					container_remove_child(container);
					container_add_child(sibling, container);
				}
				container->width = container->height = 0;
				sibling = NULL;
			}
			break;
		default:
			sway_assert(0, "Not expecting to see container of type %s here",
					container_type_to_str(sibling->type));
			return;
		}
	}

	container_notify_subtree_changed(old_parent);
	container_notify_subtree_changed(container->parent);

	if (container->type == C_VIEW) {
		ipc_event_window(container, "move");
	}

	if (old_parent) {
		seat_set_focus(config->handler_context.seat, old_parent);
		seat_set_focus(config->handler_context.seat, container);
	}

	struct sway_container *last_ws = old_parent;
	struct sway_container *next_ws = container->parent;
	if (last_ws && last_ws->type != C_WORKSPACE) {
		last_ws = container_parent(last_ws, C_WORKSPACE);
	}
	if (next_ws && next_ws->type != C_WORKSPACE) {
		next_ws = container_parent(next_ws, C_WORKSPACE);
	}
	if (last_ws && next_ws && last_ws != next_ws) {
		ipc_event_workspace(last_ws, next_ws, "focus");
		workspace_detect_urgent(last_ws);
		workspace_detect_urgent(next_ws);
	}
	container_end_mouse_operation(container);
}

static struct cmd_results *cmd_move_container(struct sway_container *current,
		int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "move container/window",
				EXPECTED_AT_LEAST, 3))) {
		return error;
	}

	if (current->type == C_WORKSPACE) {
		if (current->children->length == 0) {
			return cmd_results_new(CMD_FAILURE, "move",
					"Can't move an empty workspace");
		}
		current = workspace_wrap_children(current);
	} else if (current->type != C_CONTAINER && current->type != C_VIEW) {
		return cmd_results_new(CMD_FAILURE, "move",
				"Can only move containers and views.");
	}

	bool no_auto_back_and_forth = false;
	while (strcasecmp(argv[0], "--no-auto-back-and-forth") == 0) {
		no_auto_back_and_forth = true;
		if (--argc < 3) {
			return cmd_results_new(CMD_INVALID, "move", expected_syntax);
		}
		++argv;
	}
	while (strcasecmp(argv[1], "--no-auto-back-and-forth") == 0) {
		no_auto_back_and_forth = true;
		if (--argc < 3) {
			return cmd_results_new(CMD_INVALID, "move", expected_syntax);
		}
		argv++;
	}

	while (strcasecmp(argv[1], "to") == 0) {
		if (--argc < 3) {
			return cmd_results_new(CMD_INVALID, "move", expected_syntax);
		}
		argv++;
	}

	struct sway_container *old_parent = current->parent;
	struct sway_container *old_ws = container_parent(current, C_WORKSPACE);
	struct sway_container *destination = NULL;

	// determine destination
	if (strcasecmp(argv[1], "workspace") == 0) {
		// move container to workspace x
		struct sway_container *ws = NULL;
		char *ws_name = NULL;
		if (strcasecmp(argv[2], "next") == 0 ||
				strcasecmp(argv[2], "prev") == 0 ||
				strcasecmp(argv[2], "next_on_output") == 0 ||
				strcasecmp(argv[2], "prev_on_output") == 0 ||
				strcasecmp(argv[2], "current") == 0) {
			ws = workspace_by_name(argv[2]);
		} else if (strcasecmp(argv[2], "back_and_forth") == 0) {
			if (!(ws = workspace_by_name(argv[2]))) {
				if (prev_workspace_name) {
					ws_name = strdup(prev_workspace_name);
				} else {
					return cmd_results_new(CMD_FAILURE, "move",
							"No workspace was previously active.");
				}
			}
		} else {
			if (strcasecmp(argv[2], "number") == 0) {
				// move "container to workspace number x"
				if (argc < 4) {
					return cmd_results_new(CMD_INVALID, "move",
							expected_syntax);
				}
				if (!isdigit(argv[3][0])) {
					return cmd_results_new(CMD_INVALID, "move",
							"Invalid workspace number '%s'", argv[3]);
				}
				ws_name = join_args(argv + 3, argc - 3);
				ws = workspace_by_number(ws_name);
			} else {
				ws_name = join_args(argv + 2, argc - 2);
				ws = workspace_by_name(ws_name);
			}

			if (!no_auto_back_and_forth && config->auto_back_and_forth &&
					prev_workspace_name) {
				// auto back and forth move
				if (old_ws->name && strcmp(old_ws->name, ws_name) == 0) {
					// if target workspace is the current one
					free(ws_name);
					ws_name = strdup(prev_workspace_name);
					ws = workspace_by_name(ws_name);
				}
			}
		}
		if (!ws) {
			// We have to create the workspace, but if the container is
			// sticky and the workspace is going to be created on the same
			// output, we'll bail out first.
			if (container_is_floating(current) && current->is_sticky) {
				struct sway_container *old_output =
					container_parent(current, C_OUTPUT);
				struct sway_container *new_output =
					workspace_get_initial_output(ws_name);
				if (old_output == new_output) {
					free(ws_name);
					return cmd_results_new(CMD_FAILURE, "move",
							"Can't move sticky container to another workspace "
							"on the same output");
				}
			}
			ws = workspace_create(NULL, ws_name);
		}
		free(ws_name);
		destination = seat_get_focus_inactive(config->handler_context.seat, ws);
	} else if (strcasecmp(argv[1], "output") == 0) {
		struct sway_container *source = container_parent(current, C_OUTPUT);
		struct sway_container *dest_output = output_in_direction(argv[2],
				source->sway_output->wlr_output, current->x, current->y);
		if (!dest_output) {
			return cmd_results_new(CMD_FAILURE, "move workspace",
				"Can't find output with name/direction '%s'", argv[2]);
		}
		destination = seat_get_focus_inactive(
				config->handler_context.seat, dest_output);
		if (!destination) {
			// We've never been to this output before
			destination = dest_output->children->items[0];
		}
	} else if (strcasecmp(argv[1], "mark") == 0) {
		struct sway_view *dest_view = view_find_mark(argv[2]);
		if (dest_view == NULL) {
			return cmd_results_new(CMD_FAILURE, "move",
					"Mark '%s' not found", argv[2]);
		}
		destination = dest_view->swayc;
	} else {
		return cmd_results_new(CMD_INVALID, "move", expected_syntax);
	}

	if (container_is_floating(current) && current->is_sticky) {
		struct sway_container *old_output = container_parent(current, C_OUTPUT);
		struct sway_container *new_output = destination->type == C_OUTPUT ?
			destination : container_parent(destination, C_OUTPUT);
		if (old_output == new_output) {
			return cmd_results_new(CMD_FAILURE, "move", "Can't move sticky "
					"container to another workspace on the same output");
		}
	}

	// move container, arrange windows and return focus
	container_move_to(current, destination);
	struct sway_container *focus =
		seat_get_focus_inactive(config->handler_context.seat, old_parent);
	seat_set_focus_warp(config->handler_context.seat, focus, true, false);
	container_reap_empty(old_parent);
	container_reap_empty(destination->parent);

	// TODO: Ideally we would arrange the surviving parent after reaping,
	// but container_reap_empty does not return it, so we arrange the
	// workspace instead.
	arrange_windows(old_ws);
	arrange_windows(destination->parent);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static void workspace_move_to_output(struct sway_container *workspace,
		struct sway_container *output) {
	if (!sway_assert(workspace->type == C_WORKSPACE, "Expected a workspace")) {
		return;
	}
	if (!sway_assert(output->type == C_OUTPUT, "Expected an output")) {
		return;
	}
	if (workspace->parent == output) {
		return;
	}
	struct sway_container *old_output = container_remove_child(workspace);
	struct sway_seat *seat = input_manager_get_default_seat(input_manager);
	struct sway_container *new_output_focus =
		seat_get_focus_inactive(seat, output);

	container_add_child(output, workspace);

	// If moving the last workspace from the old output, create a new workspace
	// on the old output
	if (old_output->children->length == 0) {
		char *ws_name = workspace_next_name(old_output->name);
		struct sway_container *ws = workspace_create(old_output, ws_name);
		free(ws_name);
		seat_set_focus(seat, ws);
	}

	// Try to remove an empty workspace from the destination output.
	container_reap_empty(new_output_focus);

	output_sort_workspaces(output);
	seat_set_focus(seat, output);
	workspace_output_raise_priority(workspace, old_output, output);
	ipc_event_workspace(NULL, workspace, "move");

	container_notify_subtree_changed(old_output);
	container_notify_subtree_changed(output);
}

static struct cmd_results *cmd_move_workspace(struct sway_container *current,
		int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "move workspace", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	while (strcasecmp(argv[1], "to") == 0) {
		if (--argc < 3) {
			return cmd_results_new(CMD_INVALID, "move", expected_syntax);
		}
		++argv;
	}

	if (strcasecmp(argv[1], "output") != 0) {
		return cmd_results_new(CMD_INVALID, "move", expected_syntax);
	}

	struct sway_container *source = container_parent(current, C_OUTPUT);
	int center_x = current->width / 2 + current->x,
		center_y = current->height / 2 + current->y;
	struct sway_container *destination = output_in_direction(argv[2],
			source->sway_output->wlr_output, center_x, center_y);
	if (!destination) {
		return cmd_results_new(CMD_FAILURE, "move workspace",
			"Can't find output with name/direction '%s'", argv[2]);
	}
	if (current->type != C_WORKSPACE) {
		current = container_parent(current, C_WORKSPACE);
	}
	workspace_move_to_output(current, destination);

	arrange_windows(source);
	arrange_windows(destination);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *move_in_direction(struct sway_container *container,
		enum movement_direction direction, int argc, char **argv) {
	int move_amt = 10;
	if (argc > 1) {
		char *inv;
		move_amt = (int)strtol(argv[1], &inv, 10);
		if (*inv != '\0' && strcasecmp(inv, "px") != 0) {
			return cmd_results_new(CMD_FAILURE, "move",
					"Invalid distance specified");
		}
	}

	if (container->type == C_WORKSPACE) {
		return cmd_results_new(CMD_FAILURE, "move",
				"Cannot move workspaces in a direction");
	}
	if (container_is_floating(container)) {
		if (container->is_fullscreen) {
			return cmd_results_new(CMD_FAILURE, "move",
					"Cannot move fullscreen floating container");
		}
		double lx = container->x;
		double ly = container->y;
		switch (direction) {
		case MOVE_LEFT:
			lx -= move_amt;
			break;
		case MOVE_RIGHT:
			lx += move_amt;
			break;
		case MOVE_UP:
			ly -= move_amt;
			break;
		case MOVE_DOWN:
			ly += move_amt;
			break;
		case MOVE_PARENT:
		case MOVE_CHILD:
			return cmd_results_new(CMD_FAILURE, "move",
					"Cannot move floating container to parent or child");
		}
		container_floating_move_to(container, lx, ly);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}
	// For simplicity, we'll arrange the entire workspace. The reason for this
	// is moving the container might reap the old parent, and container_move
	// does not return a surviving parent.
	// TODO: Make container_move return the surviving parent so we can arrange
	// just that.
	struct sway_container *old_ws = container_parent(container, C_WORKSPACE);
	container_move(container, direction, move_amt);
	struct sway_container *new_ws = container_parent(container, C_WORKSPACE);

	arrange_windows(old_ws);
	if (new_ws != old_ws) {
		arrange_windows(new_ws);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static const char *expected_position_syntax =
	"Expected 'move [absolute] position <x> [px] <y> [px]' or "
	"'move [absolute] position center' or "
	"'move position cursor|mouse|pointer'";

static struct cmd_results *move_to_position(struct sway_container *container,
		int argc, char **argv) {
	if (!container_is_floating(container)) {
		return cmd_results_new(CMD_FAILURE, "move",
				"Only floating containers "
				"can be moved to an absolute position");
	}
	if (!argc) {
		return cmd_results_new(CMD_FAILURE, "move", expected_position_syntax);
	}

	bool absolute = false;
	if (strcmp(argv[0], "absolute") == 0) {
		absolute = true;
		--argc;
		++argv;
	}
	if (!argc) {
		return cmd_results_new(CMD_FAILURE, "move", expected_position_syntax);
	}
	if (strcmp(argv[0], "position") == 0) {
		--argc;
		++argv;
	}
	if (!argc) {
		return cmd_results_new(CMD_FAILURE, "move", expected_position_syntax);
	}
	if (strcmp(argv[0], "cursor") == 0 || strcmp(argv[0], "mouse") == 0 ||
			strcmp(argv[0], "pointer") == 0) {
		struct sway_seat *seat = config->handler_context.seat;
		if (!seat->cursor) {
			return cmd_results_new(CMD_FAILURE, "move", "No cursor device");
		}
		double lx = seat->cursor->cursor->x - container->width / 2;
		double ly = seat->cursor->cursor->y - container->height / 2;
		container_floating_move_to(container, lx, ly);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	} else if (strcmp(argv[0], "center") == 0) {
		double lx, ly;
		if (absolute) {
			lx = root_container.x + (root_container.width - container->width) / 2;
			ly = root_container.y + (root_container.height - container->height) / 2;
		} else {
			struct sway_container *ws = container_parent(container, C_WORKSPACE);
			lx = ws->x + (ws->width - container->width) / 2;
			ly = ws->y + (ws->height - container->height) / 2;
		}
		container_floating_move_to(container, lx, ly);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}

	if (argc < 2) {
		return cmd_results_new(CMD_FAILURE, "move", expected_position_syntax);
	}

	double lx, ly;
	char *inv;
	lx = (double)strtol(argv[0], &inv, 10);
	if (*inv != '\0' && strcasecmp(inv, "px") != 0) {
		return cmd_results_new(CMD_FAILURE, "move",
				"Invalid position specified");
	}
	if (strcmp(argv[1], "px") == 0) {
		--argc;
		++argv;
	}

	if (argc > 3) {
		return cmd_results_new(CMD_FAILURE, "move", expected_position_syntax);
	}

	ly = (double)strtol(argv[1], &inv, 10);
	if ((*inv != '\0' && strcasecmp(inv, "px") != 0) ||
			(argc == 3 && strcmp(argv[2], "px") != 0)) {
		return cmd_results_new(CMD_FAILURE, "move",
				"Invalid position specified");
	}

	if (!absolute) {
		struct sway_container *ws = container_parent(container, C_WORKSPACE);
		lx += ws->x;
		ly += ws->y;
	}
	container_floating_move_to(container, lx, ly);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *move_to_scratchpad(struct sway_container *con) {
	if (con->type == C_WORKSPACE && con->children->length == 0) {
		return cmd_results_new(CMD_INVALID, "move",
				"Can't move an empty workspace to the scratchpad");
	}
	if (con->type == C_WORKSPACE) {
		// Wrap the workspace's children in a container
		struct sway_container *workspace = con;
		con = workspace_wrap_children(con);
		workspace->layout = L_HORIZ;
	}

	// If the container is in a floating split container,
	// operate on the split container instead of the child.
	if (container_is_floating_or_child(con)) {
		while (con->parent->type != C_WORKSPACE) {
			con = con->parent;
		}
	}

	if (con->scratchpad) {
		return cmd_results_new(CMD_INVALID, "move",
				"Container is already in the scratchpad");
	}
	root_scratchpad_add_container(con);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_move(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "move", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct sway_container *current = config->handler_context.current_container;

	if (strcasecmp(argv[0], "left") == 0) {
		return move_in_direction(current, MOVE_LEFT, argc, argv);
	} else if (strcasecmp(argv[0], "right") == 0) {
		return move_in_direction(current, MOVE_RIGHT, argc, argv);
	} else if (strcasecmp(argv[0], "up") == 0) {
		return move_in_direction(current, MOVE_UP, argc, argv);
	} else if (strcasecmp(argv[0], "down") == 0) {
		return move_in_direction(current, MOVE_DOWN, argc, argv);
	} else if ((strcasecmp(argv[0], "container") == 0
			|| strcasecmp(argv[0], "window") == 0) ||
			(strcasecmp(argv[0], "--no-auto-back-and-forth") &&
			(strcasecmp(argv[0], "container") == 0
			|| strcasecmp(argv[0], "window") == 0))) {
		return cmd_move_container(current, argc, argv);
	} else if (strcasecmp(argv[0], "workspace") == 0) {
		return cmd_move_workspace(current, argc, argv);
	} else if (strcasecmp(argv[0], "scratchpad") == 0
			|| (strcasecmp(argv[0], "to") == 0 && argc == 2
				&& strcasecmp(argv[1], "scratchpad") == 0)) {
		return move_to_scratchpad(current);
	} else if (strcasecmp(argv[0], "position") == 0) {
		return move_to_position(current, argc, argv);
	} else if (strcasecmp(argv[0], "absolute") == 0) {
		return move_to_position(current, argc, argv);
	} else {
		return cmd_results_new(CMD_INVALID, "move", expected_syntax);
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
