#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
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
#include "util.h"

static const char expected_syntax[] =
	"Expected 'move <left|right|up|down> <[px] px>' or "
	"'move [--no-auto-back-and-forth] <container|window> [to] workspace <name>' or "
	"'move <container|window|workspace> [to] output <name|direction>' or "
	"'move <container|window> [to] mark <mark>'";

static struct sway_output *output_in_direction(const char *direction_string,
		struct sway_output *reference, int ref_lx, int ref_ly) {
	struct {
		char *name;
		enum wlr_direction direction;
	} names[] = {
		{ "up", WLR_DIRECTION_UP },
		{ "down", WLR_DIRECTION_DOWN },
		{ "left", WLR_DIRECTION_LEFT },
		{ "right", WLR_DIRECTION_RIGHT },
	};

	enum wlr_direction direction = 0;

	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
		if (strcasecmp(names[i].name, direction_string) == 0) {
			direction = names[i].direction;
			break;
		}
	}

	if (reference && direction) {
		struct wlr_output *target = wlr_output_layout_adjacent_output(
				root->output_layout, direction, reference->wlr_output,
				ref_lx, ref_ly);

		if (!target) {
			target = wlr_output_layout_farthest_output(
					root->output_layout, opposite_direction(direction),
					reference->wlr_output, ref_lx, ref_ly);
		}

		if (target) {
			return target->data;
		}
	}

	return output_by_name_or_id(direction_string);
}

static bool is_parallel(enum sway_container_layout layout,
		enum wlr_direction dir) {
	switch (layout) {
	case L_TABBED:
	case L_HORIZ:
		return dir == WLR_DIRECTION_LEFT || dir == WLR_DIRECTION_RIGHT;
	case L_STACKED:
	case L_VERT:
		return dir == WLR_DIRECTION_UP || dir == WLR_DIRECTION_DOWN;
	default:
		return false;
	}
}

/**
 * Ensures all seats focus the fullscreen container if needed.
 */
static void workspace_focus_fullscreen(struct sway_workspace *workspace) {
	if (!workspace->fullscreen) {
		return;
	}
	struct sway_seat *seat;
	struct sway_workspace *focus_ws;
	wl_list_for_each(seat, &server.input->seats, link) {
		focus_ws = seat_get_focused_workspace(seat);
		if (focus_ws == workspace) {
			struct sway_node *new_focus =
				seat_get_focus_inactive(seat, &workspace->fullscreen->node);
			seat_set_raw_focus(seat, new_focus);
		}
	}
}

static void container_move_to_container_from_direction(
		struct sway_container *container, struct sway_container *destination,
		enum wlr_direction move_dir) {
	if (destination->view) {
		if (destination->parent == container->parent &&
				destination->workspace == container->workspace) {
			sway_log(SWAY_DEBUG, "Swapping siblings");
			list_t *siblings = container_get_siblings(container);
			int container_index = list_find(siblings, container);
			int destination_index = list_find(siblings, destination);
			list_swap(siblings, container_index, destination_index);
		} else {
			sway_log(SWAY_DEBUG, "Promoting to sibling of cousin");
			int offset =
				move_dir == WLR_DIRECTION_LEFT || move_dir == WLR_DIRECTION_UP;
			int index = container_sibling_index(destination) + offset;
			if (destination->parent) {
				container_insert_child(destination->parent, container, index);
			} else {
				workspace_insert_tiling(destination->workspace,
						container, index);
			}
			container->width = container->height = 0;
		}
		return;
	}

	if (is_parallel(destination->layout, move_dir)) {
		sway_log(SWAY_DEBUG, "Reparenting container (parallel)");
		int index =
			move_dir == WLR_DIRECTION_RIGHT || move_dir == WLR_DIRECTION_DOWN ?
			0 : destination->children->length;
		container_insert_child(destination, container, index);
		container->width = container->height = 0;
		return;
	}

	sway_log(SWAY_DEBUG, "Reparenting container (perpendicular)");
	struct sway_node *focus_inactive = seat_get_active_tiling_child(
			config->handler_context.seat, &destination->node);
	if (!focus_inactive || focus_inactive == &destination->node) {
		// The container has no children
		container_add_child(destination, container);
		return;
	}

	// Try again but with the child
	container_move_to_container_from_direction(container,
			focus_inactive->sway_container, move_dir);
}

static void container_move_to_workspace_from_direction(
		struct sway_container *container, struct sway_workspace *workspace,
		enum wlr_direction move_dir) {
	container->width = container->height = 0;

	if (is_parallel(workspace->layout, move_dir)) {
		sway_log(SWAY_DEBUG, "Reparenting container (parallel)");
		int index =
			move_dir == WLR_DIRECTION_RIGHT || move_dir == WLR_DIRECTION_DOWN ?
			0 : workspace->tiling->length;
		workspace_insert_tiling(workspace, container, index);
		return;
	}

	sway_log(SWAY_DEBUG, "Reparenting container (perpendicular)");
	struct sway_container *focus_inactive = seat_get_focus_inactive_tiling(
			config->handler_context.seat, workspace);
	if (!focus_inactive) {
		// The workspace has no tiling children
		workspace_add_tiling(workspace, container);
		return;
	}
	while (focus_inactive->parent) {
		focus_inactive = focus_inactive->parent;
	}
	container_move_to_container_from_direction(container, focus_inactive,
			move_dir);
}

static void container_move_to_workspace(struct sway_container *container,
		struct sway_workspace *workspace) {
	if (container->workspace == workspace) {
		return;
	}
	struct sway_workspace *old_workspace = container->workspace;
	if (container_is_floating(container)) {
		struct sway_output *old_output = container->workspace->output;
		container_detach(container);
		workspace_add_floating(workspace, container);
		container_handle_fullscreen_reparent(container);
		// If changing output, center it within the workspace
		if (old_output != workspace->output && !container->fullscreen_mode) {
			container_floating_move_to_center(container);
		}
	} else {
		container_detach(container);
		container->width = container->height = 0;
		container->saved_width = container->saved_height = 0;
		workspace_add_tiling(workspace, container);
		container_update_representation(container);
	}
	if (container->view) {
		ipc_event_window(container, "move");
	}
	workspace_detect_urgent(old_workspace);
	workspace_detect_urgent(workspace);
	workspace_focus_fullscreen(workspace);
}

static void container_move_to_container(struct sway_container *container,
		struct sway_container *destination) {
	if (container == destination
			|| container_has_ancestor(container, destination)
			|| container_has_ancestor(destination, container)) {
		return;
	}
	if (container_is_floating(container)) {
		container_move_to_workspace(container, destination->workspace);
		return;
	}
	struct sway_workspace *old_workspace = container->workspace;

	container_detach(container);
	container_remove_gaps(container);
	container->width = container->height = 0;
	container->saved_width = container->saved_height = 0;

	if (destination->view) {
		container_add_sibling(destination, container, 1);
	} else {
		container_add_child(destination, container);
	}

	if (container->view) {
		ipc_event_window(container, "move");
	}

	workspace_focus_fullscreen(destination->workspace);

	// Update workspace urgent state
	workspace_detect_urgent(destination->workspace);
	if (old_workspace && old_workspace != destination->workspace) {
		workspace_detect_urgent(old_workspace);
	}
}

/* Takes one child, sets it aside, wraps the rest of the children in a new
 * container, switches the layout of the workspace, and drops the child back in.
 * In other words, rejigger it. */
static void workspace_rejigger(struct sway_workspace *ws,
		struct sway_container *child, enum wlr_direction move_dir) {
	if (!child->parent && ws->tiling->length == 1) {
		ws->layout =
			move_dir == WLR_DIRECTION_LEFT || move_dir == WLR_DIRECTION_RIGHT ?
			L_HORIZ : L_VERT;
		workspace_update_representation(ws);
		return;
	}
	container_detach(child);
	struct sway_container *new_parent = workspace_wrap_children(ws);

	int index =
		move_dir == WLR_DIRECTION_LEFT || move_dir == WLR_DIRECTION_UP ? 0 : 1;
	workspace_insert_tiling(ws, child, index);
	container_flatten(new_parent);
	ws->layout =
		move_dir == WLR_DIRECTION_LEFT || move_dir == WLR_DIRECTION_RIGHT ?
		L_HORIZ : L_VERT;
	workspace_update_representation(ws);
	child->width = child->height = 0;
}

// Returns true if moved
static bool container_move_in_direction(struct sway_container *container,
		enum wlr_direction move_dir) {
	// If moving a fullscreen view, only consider outputs
	if (container->fullscreen_mode == FULLSCREEN_WORKSPACE) {
		struct sway_output *new_output =
			output_get_in_direction(container->workspace->output, move_dir);
		if (!new_output) {
			return false;
		}
		struct sway_workspace *ws = output_get_active_workspace(new_output);
		container_move_to_workspace(container, ws);
		return true;
	}
	if (container->fullscreen_mode == FULLSCREEN_GLOBAL) {
		return false;
	}

	// If container is in a split container by itself, move out of the split
	if (container->parent) {
		struct sway_container *new_parent =
			container_flatten(container->parent);
		if (new_parent != container->parent) {
			return true;
		}
	}

	// Look for a suitable *container* sibling or parent.
	// The below loop stops once we hit the workspace because current->parent
	// is NULL for the topmost containers in a workspace.
	struct sway_container *current = container;
	int offs =
		move_dir == WLR_DIRECTION_LEFT || move_dir == WLR_DIRECTION_UP ? -1 : 1;

	while (current) {
		list_t *siblings = container_get_siblings(current);
		enum sway_container_layout layout = container_parent_layout(current);
		int index = list_find(siblings, current);
		int desired = index + offs;

		// Don't allow containers to move out of their
		// fullscreen or floating parent
		if (current->fullscreen_mode || container_is_floating(current)) {
			return false;
		}

		if (is_parallel(layout, move_dir)) {
			if (desired == -1 || desired == siblings->length) {
				if (current->parent == container->parent) {
					current = current->parent;
					continue;
				} else {
					// Reparenting
					if (current->parent) {
						container_insert_child(current->parent, container,
								index + (offs < 0 ? 0 : 1));
					} else {
						workspace_insert_tiling(current->workspace, container,
								index + (offs < 0 ? 0 : 1));
					}
					return true;
				}
			} else {
				// Container can move within its siblings
				container_move_to_container_from_direction(container,
						siblings->items[desired], move_dir);
				return true;
			}
		}

		current = current->parent;
	}

	// Maybe rejigger the workspace
	struct sway_workspace *ws = container->workspace;
	if (!is_parallel(ws->layout, move_dir)) {
		workspace_rejigger(ws, container, move_dir);
		return true;
	} else if (ws->layout == L_TABBED || ws->layout == L_STACKED) {
		workspace_rejigger(ws, container, move_dir);
		return true;
	}

	// Try adjacent output
	struct sway_output *output =
		output_get_in_direction(container->workspace->output, move_dir);
	if (output) {
		struct sway_workspace *ws = output_get_active_workspace(output);
		container_move_to_workspace_from_direction(container, ws, move_dir);
		return true;
	}
	sway_log(SWAY_DEBUG, "Hit edge of output, nowhere else to go");
	return false;
}

static struct cmd_results *cmd_move_container(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "move container/window",
				EXPECTED_AT_LEAST, 3))) {
		return error;
	}

	struct sway_node *node = config->handler_context.node;
	struct sway_workspace *workspace = config->handler_context.workspace;
	struct sway_container *container = config->handler_context.container;
	if (node->type == N_WORKSPACE) {
		if (workspace->tiling->length == 0) {
			return cmd_results_new(CMD_FAILURE,
					"Can't move an empty workspace");
		}
		container = workspace_wrap_children(workspace);
	}

	bool no_auto_back_and_forth = false;
	while (strcasecmp(argv[0], "--no-auto-back-and-forth") == 0) {
		no_auto_back_and_forth = true;
		if (--argc < 3) {
			return cmd_results_new(CMD_INVALID, expected_syntax);
		}
		++argv;
	}
	while (strcasecmp(argv[1], "--no-auto-back-and-forth") == 0) {
		no_auto_back_and_forth = true;
		if (--argc < 3) {
			return cmd_results_new(CMD_INVALID, expected_syntax);
		}
		argv++;
	}

	while (strcasecmp(argv[1], "to") == 0) {
		if (--argc < 3) {
			return cmd_results_new(CMD_INVALID, expected_syntax);
		}
		argv++;
	}

	struct sway_seat *seat = config->handler_context.seat;
	struct sway_container *old_parent = container->parent;
	struct sway_workspace *old_ws = container->workspace;
	struct sway_output *old_output = old_ws ? old_ws->output : NULL;
	struct sway_node *destination = NULL;

	// determine destination
	if (strcasecmp(argv[1], "workspace") == 0) {
		// move container to workspace x
		struct sway_workspace *ws = NULL;
		char *ws_name = NULL;
		if (strcasecmp(argv[2], "next") == 0 ||
				strcasecmp(argv[2], "prev") == 0 ||
				strcasecmp(argv[2], "next_on_output") == 0 ||
				strcasecmp(argv[2], "prev_on_output") == 0 ||
				strcasecmp(argv[2], "current") == 0) {
			ws = workspace_by_name(argv[2]);
		} else if (strcasecmp(argv[2], "back_and_forth") == 0) {
			if (!(ws = workspace_by_name(argv[2]))) {
				if (seat->prev_workspace_name) {
					ws_name = strdup(seat->prev_workspace_name);
				} else {
					return cmd_results_new(CMD_FAILURE,
							"No workspace was previously active.");
				}
			}
		} else {
			if (strcasecmp(argv[2], "number") == 0) {
				// move "container to workspace number x"
				if (argc < 4) {
					return cmd_results_new(CMD_INVALID, expected_syntax);
				}
				if (!isdigit(argv[3][0])) {
					return cmd_results_new(CMD_INVALID,
							"Invalid workspace number '%s'", argv[3]);
				}
				ws_name = join_args(argv + 3, argc - 3);
				ws = workspace_by_number(ws_name);
			} else {
				ws_name = join_args(argv + 2, argc - 2);
				ws = workspace_by_name(ws_name);
			}

			if (!no_auto_back_and_forth && config->auto_back_and_forth &&
					seat->prev_workspace_name) {
				// auto back and forth move
				if (old_ws && old_ws->name &&
						strcmp(old_ws->name, ws_name) == 0) {
					// if target workspace is the current one
					free(ws_name);
					ws_name = strdup(seat->prev_workspace_name);
					ws = workspace_by_name(ws_name);
				}
			}
		}
		if (!ws) {
			// We have to create the workspace, but if the container is
			// sticky and the workspace is going to be created on the same
			// output, we'll bail out first.
			if (container->is_sticky) {
				struct sway_output *new_output =
					workspace_get_initial_output(ws_name);
				if (old_output == new_output) {
					free(ws_name);
					return cmd_results_new(CMD_FAILURE,
							"Can't move sticky container to another workspace "
							"on the same output");
				}
			}
			ws = workspace_create(NULL, ws_name);
		}
		free(ws_name);
		struct sway_container *dst = seat_get_focus_inactive_tiling(seat, ws);
		destination = dst ? &dst->node : &ws->node;
	} else if (strcasecmp(argv[1], "output") == 0) {
		struct sway_output *new_output = output_in_direction(argv[2],
				old_output, container->x, container->y);
		if (!new_output) {
			return cmd_results_new(CMD_FAILURE,
				"Can't find output with name/direction '%s'", argv[2]);
		}
		destination = seat_get_focus_inactive(seat, &new_output->node);
	} else if (strcasecmp(argv[1], "mark") == 0) {
		struct sway_container *dest_con = container_find_mark(argv[2]);
		if (dest_con == NULL) {
			return cmd_results_new(CMD_FAILURE,
					"Mark '%s' not found", argv[2]);
		}
		destination = &dest_con->node;
	} else {
		return cmd_results_new(CMD_INVALID, expected_syntax);
	}

	if (container->is_sticky && old_output &&
			node_has_ancestor(destination, &old_output->node)) {
		return cmd_results_new(CMD_FAILURE, "Can't move sticky "
				"container to another workspace on the same output");
	}

	struct sway_output *new_output = node_get_output(destination);
	struct sway_workspace *new_output_last_ws = old_output == new_output ?
		NULL : output_get_active_workspace(new_output);

	// save focus, in case it needs to be restored
	struct sway_node *focus = seat_get_focus(seat);

	// move container
	if (container->scratchpad) {
		root_scratchpad_remove_container(container);
		root_scratchpad_show(container);
	}
	switch (destination->type) {
	case N_WORKSPACE:
		container_move_to_workspace(container, destination->sway_workspace);
		break;
	case N_OUTPUT: {
			struct sway_output *output = destination->sway_output;
			struct sway_workspace *ws = output_get_active_workspace(output);
			container_move_to_workspace(container, ws);
		}
		break;
	case N_CONTAINER:
		container_move_to_container(container, destination->sway_container);
		break;
	case N_ROOT:
		break;
	}

	// restore focus on destination output back to its last active workspace
	struct sway_workspace *new_workspace =
		output_get_active_workspace(new_output);
	if (new_output_last_ws && new_output_last_ws != new_workspace) {
		struct sway_node *new_output_last_focus =
			seat_get_focus_inactive(seat, &new_output_last_ws->node);
		seat_set_raw_focus(seat, new_output_last_focus);
	}

	// restore focus
	if (focus == &container->node) {
		focus = NULL;
		if (old_parent) {
			focus = seat_get_focus_inactive(seat, &old_parent->node);
		}
		if (!focus && old_ws) {
			focus = seat_get_focus_inactive(seat, &old_ws->node);
		}
	}
	seat_set_focus(seat, focus);

	// clean-up, destroying parents if the container was the last child
	if (old_parent) {
		container_reap_empty(old_parent);
	} else if (old_ws) {
		workspace_consider_destroy(old_ws);
	}

	// arrange windows
	if (root->fullscreen_global) {
		arrange_root();
	} else {
		if (old_ws && !old_ws->node.destroying) {
			arrange_workspace(old_ws);
		}
		arrange_node(node_get_parent(destination));
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

static void workspace_move_to_output(struct sway_workspace *workspace,
		struct sway_output *output) {
	if (workspace->output == output) {
		return;
	}
	struct sway_output *old_output = workspace->output;
	workspace_detach(workspace);
	struct sway_workspace *new_output_old_ws =
		output_get_active_workspace(output);

	output_add_workspace(output, workspace);

	// If moving the last workspace from the old output, create a new workspace
	// on the old output
	struct sway_seat *seat = config->handler_context.seat;
	if (old_output->workspaces->length == 0) {
		char *ws_name = workspace_next_name(old_output->wlr_output->name);
		struct sway_workspace *ws = workspace_create(old_output, ws_name);
		free(ws_name);
		seat_set_raw_focus(seat, &ws->node);
	}

	workspace_consider_destroy(new_output_old_ws);

	output_sort_workspaces(output);
	struct sway_node *focus = seat_get_focus_inactive(seat, &workspace->node);
	seat_set_focus(seat, focus);
	workspace_output_raise_priority(workspace, old_output, output);
	ipc_event_workspace(NULL, workspace, "move");
}

static struct cmd_results *cmd_move_workspace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "move workspace", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	while (strcasecmp(argv[1], "to") == 0) {
		if (--argc < 3) {
			return cmd_results_new(CMD_INVALID, expected_syntax);
		}
		++argv;
	}

	if (strcasecmp(argv[1], "output") != 0) {
		return cmd_results_new(CMD_INVALID, expected_syntax);
	}

	struct sway_workspace *workspace = config->handler_context.workspace;
	struct sway_output *old_output = workspace->output;
	int center_x = workspace->width / 2 + workspace->x,
		center_y = workspace->height / 2 + workspace->y;
	struct sway_output *new_output = output_in_direction(argv[2],
			old_output, center_x, center_y);
	if (!new_output) {
		return cmd_results_new(CMD_FAILURE,
			"Can't find output with name/direction '%s'", argv[2]);
	}
	workspace_move_to_output(workspace, new_output);

	arrange_output(old_output);
	arrange_output(new_output);

	return cmd_results_new(CMD_SUCCESS, NULL);
}

static struct cmd_results *cmd_move_in_direction(
		enum wlr_direction direction, int argc, char **argv) {
	int move_amt = 10;
	if (argc > 1) {
		char *inv;
		move_amt = (int)strtol(argv[1], &inv, 10);
		if (*inv != '\0' && strcasecmp(inv, "px") != 0) {
			return cmd_results_new(CMD_FAILURE, "Invalid distance specified");
		}
	}

	struct sway_container *container = config->handler_context.container;
	if (!container) {
		return cmd_results_new(CMD_FAILURE,
				"Cannot move workspaces in a direction");
	}
	if (container_is_scratchpad_hidden(container)) {
		return cmd_results_new(CMD_FAILURE,
				"Cannot move a hidden scratchpad container");
	}
	if (container_is_floating(container)) {
		if (container->fullscreen_mode) {
			return cmd_results_new(CMD_FAILURE,
					"Cannot move fullscreen floating container");
		}
		double lx = container->x;
		double ly = container->y;
		switch (direction) {
		case WLR_DIRECTION_LEFT:
			lx -= move_amt;
			break;
		case WLR_DIRECTION_RIGHT:
			lx += move_amt;
			break;
		case WLR_DIRECTION_UP:
			ly -= move_amt;
			break;
		case WLR_DIRECTION_DOWN:
			ly += move_amt;
			break;
		}
		container_floating_move_to(container, lx, ly);
		return cmd_results_new(CMD_SUCCESS, NULL);
	}
	struct sway_workspace *old_ws = container->workspace;

	if (!container_move_in_direction(container, direction)) {
		// Container didn't move
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	struct sway_workspace *new_ws = container->workspace;

	if (root->fullscreen_global) {
		arrange_root();
	} else {
		arrange_workspace(old_ws);
		if (new_ws != old_ws) {
			arrange_workspace(new_ws);
		}
	}

	if (container->view) {
		ipc_event_window(container, "move");
	}

	// Hack to re-focus container
	seat_set_raw_focus(config->handler_context.seat, &new_ws->node);
	seat_set_focus_container(config->handler_context.seat, container);

	if (old_ws != new_ws) {
		ipc_event_workspace(old_ws, new_ws, "focus");
		workspace_detect_urgent(old_ws);
		workspace_detect_urgent(new_ws);
	}
	container_end_mouse_operation(container);

	return cmd_results_new(CMD_SUCCESS, NULL);
}

static const char expected_position_syntax[] =
	"Expected 'move [absolute] position <x> [px] <y> [px]' or "
	"'move [absolute] position center' or "
	"'move position cursor|mouse|pointer'";

static struct cmd_results *cmd_move_to_position(int argc, char **argv) {
	struct sway_container *container = config->handler_context.container;
	if (!container || !container_is_floating(container)) {
		return cmd_results_new(CMD_FAILURE, "Only floating containers "
				"can be moved to an absolute position");
	}
	if (container_is_scratchpad_hidden(container)) {
		return cmd_results_new(CMD_FAILURE,
				"Cannot move a hidden scratchpad container");
	}

	if (!argc) {
		return cmd_results_new(CMD_FAILURE, expected_position_syntax);
	}

	bool absolute = false;
	if (strcmp(argv[0], "absolute") == 0) {
		absolute = true;
		--argc;
		++argv;
	}
	if (!argc) {
		return cmd_results_new(CMD_FAILURE, expected_position_syntax);
	}
	if (strcmp(argv[0], "position") == 0) {
		--argc;
		++argv;
	}
	if (!argc) {
		return cmd_results_new(CMD_FAILURE, expected_position_syntax);
	}
	if (strcmp(argv[0], "cursor") == 0 || strcmp(argv[0], "mouse") == 0 ||
			strcmp(argv[0], "pointer") == 0) {
		struct sway_seat *seat = config->handler_context.seat;
		if (!seat->cursor) {
			return cmd_results_new(CMD_FAILURE, "No cursor device");
		}
		double lx = seat->cursor->cursor->x - container->width / 2;
		double ly = seat->cursor->cursor->y - container->height / 2;
		container_floating_move_to(container, lx, ly);
		return cmd_results_new(CMD_SUCCESS, NULL);
	} else if (strcmp(argv[0], "center") == 0) {
		double lx, ly;
		if (absolute) {
			lx = root->x + (root->width - container->width) / 2;
			ly = root->y + (root->height - container->height) / 2;
		} else {
			struct sway_workspace *ws = container->workspace;
			lx = ws->x + (ws->width - container->width) / 2;
			ly = ws->y + (ws->height - container->height) / 2;
		}
		container_floating_move_to(container, lx, ly);
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	if (argc < 2) {
		return cmd_results_new(CMD_FAILURE, expected_position_syntax);
	}

	double lx, ly;
	char *inv;
	lx = (double)strtol(argv[0], &inv, 10);
	if (*inv != '\0' && strcasecmp(inv, "px") != 0) {
		return cmd_results_new(CMD_FAILURE, "Invalid position specified");
	}
	if (strcmp(argv[1], "px") == 0) {
		--argc;
		++argv;
	}

	if (argc > 3) {
		return cmd_results_new(CMD_FAILURE, expected_position_syntax);
	}

	ly = (double)strtol(argv[1], &inv, 10);
	if ((*inv != '\0' && strcasecmp(inv, "px") != 0) ||
			(argc == 3 && strcmp(argv[2], "px") != 0)) {
		return cmd_results_new(CMD_FAILURE, "Invalid position specified");
	}

	if (!absolute) {
		lx += container->workspace->x;
		ly += container->workspace->y;
	}
	container_floating_move_to(container, lx, ly);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

static struct cmd_results *cmd_move_to_scratchpad(void) {
	struct sway_node *node = config->handler_context.node;
	struct sway_container *con = config->handler_context.container;
	struct sway_workspace *ws = config->handler_context.workspace;
	if (node->type == N_WORKSPACE && ws->tiling->length == 0) {
		return cmd_results_new(CMD_INVALID,
				"Can't move an empty workspace to the scratchpad");
	}
	if (node->type == N_WORKSPACE) {
		// Wrap the workspace's children in a container
		con = workspace_wrap_children(ws);
		ws->layout = L_HORIZ;
	}

	// If the container is in a floating split container,
	// operate on the split container instead of the child.
	if (container_is_floating_or_child(con)) {
		while (con->parent) {
			con = con->parent;
		}
	}

	if (con->scratchpad) {
		return cmd_results_new(CMD_INVALID,
				"Container is already in the scratchpad");
	}
	root_scratchpad_add_container(con);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_move(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "move", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!root->outputs->length) {
		return cmd_results_new(CMD_INVALID,
				"Can't run this command while there's no outputs connected.");
	}

	if (strcasecmp(argv[0], "left") == 0) {
		return cmd_move_in_direction(WLR_DIRECTION_LEFT, argc, argv);
	} else if (strcasecmp(argv[0], "right") == 0) {
		return cmd_move_in_direction(WLR_DIRECTION_RIGHT, argc, argv);
	} else if (strcasecmp(argv[0], "up") == 0) {
		return cmd_move_in_direction(WLR_DIRECTION_UP, argc, argv);
	} else if (strcasecmp(argv[0], "down") == 0) {
		return cmd_move_in_direction(WLR_DIRECTION_DOWN, argc, argv);
	} else if ((strcasecmp(argv[0], "container") == 0
			|| strcasecmp(argv[0], "window") == 0) ||
			(strcasecmp(argv[0], "--no-auto-back-and-forth") && argc >= 2
			&& (strcasecmp(argv[1], "container") == 0
			|| strcasecmp(argv[1], "window") == 0))) {
		return cmd_move_container(argc, argv);
	} else if (strcasecmp(argv[0], "workspace") == 0) {
		return cmd_move_workspace(argc, argv);
	} else if (strcasecmp(argv[0], "scratchpad") == 0
			|| (strcasecmp(argv[0], "to") == 0 && argc == 2
				&& strcasecmp(argv[1], "scratchpad") == 0)) {
		return cmd_move_to_scratchpad();
	} else if (strcasecmp(argv[0], "position") == 0) {
		return cmd_move_to_position(argc, argv);
	} else if (strcasecmp(argv[0], "absolute") == 0) {
		return cmd_move_to_position(argc, argv);
	} else {
		return cmd_results_new(CMD_INVALID, expected_syntax);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
