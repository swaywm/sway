#include <strings.h>
#include <ctype.h>
#include "config.h"
#include "log.h"
#include "sway/commands.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "stringop.h"

static const char expected_syntax[] =
	"Expected 'swap (container with id|con_id|mark <arg>) |\n\t(wokspace with [number] <name>)'";

/*
 * prepare the containers inside a workspace to be moved to another
 * workspace. This includes setting a few properties of the containers
 * as well as ipc events
 */

static void handle_container_after_move(struct sway_container *container,
		void *data) {
	node_set_dirty(&container->node);
	container_update_representation(container);

	struct sway_workspace *old_workspace = container->pending.workspace;
	struct sway_output *old_output = container->pending.workspace->output;

	struct sway_workspace *destination = data;
	container->pending.workspace = destination;

	// handle floating containers here by updating their position
	if (container_is_floating(container)) {
		if (old_output == destination->output ||
				container->pending.fullscreen_mode) return;

		struct wlr_box workspace_box, old_workspace_box;

		workspace_get_box(destination, &workspace_box);
		workspace_get_box(old_workspace, &old_workspace_box);

		floating_fix_coordinates(container,
				&old_workspace_box, &workspace_box);

		if (!container->scratchpad || !destination->output) return;

		struct wlr_box output_box;
		output_get_box(destination->output, &output_box);
		container->transform = workspace_box;
	}

	if (container->view) {
		ipc_event_window(container, "move");
	}
}


/*
 * swap the properties necessary to preserve the layout
 * as well as their respective contents
 */

static void swap_workspace_properties(struct sway_workspace *cur_ws,
		struct sway_workspace *oth_ws) {
	struct sway_workspace cur_ws_cpy = *cur_ws;

	cur_ws->tiling = oth_ws->tiling;
	oth_ws->tiling = cur_ws_cpy.tiling;
	cur_ws->floating = oth_ws->floating;
	oth_ws->floating = cur_ws_cpy.floating;
	cur_ws->layout = oth_ws->layout;
	oth_ws->layout = cur_ws_cpy.layout;
	cur_ws->prev_split_layout = oth_ws->prev_split_layout;
	oth_ws->prev_split_layout = cur_ws_cpy.prev_split_layout;
	cur_ws->current_gaps = oth_ws->current_gaps;
	oth_ws->current_gaps = cur_ws_cpy.current_gaps;
	cur_ws->gaps_outer = oth_ws->gaps_outer;
	oth_ws->gaps_outer = cur_ws_cpy.gaps_outer;
	cur_ws->gaps_inner = oth_ws->gaps_inner;
	oth_ws->gaps_inner = cur_ws_cpy.gaps_inner;
}

static void set_new_focus(struct sway_workspace *ws, struct sway_seat *seat) {
	if (ws->tiling->length) {
		// this needs to be more specific (focus not just every container,
		// but single windows
		struct sway_container *container = ws->tiling->items[0];
		struct sway_container *first_view = container_get_first_view(container);

		if (!first_view) {
			seat_set_focus(seat, &ws->node);
		}

		seat_set_focus(seat, &first_view->node);
	} else if (ws->floating->length) {
		seat_set_focus(seat, ws->floating->items[0]);
	} else {
		seat_set_focus(seat, &ws->node);
	}
}

static struct cmd_results *swap_workspaces(int argc, char **argv) {
	char *ws_name = NULL;
	struct sway_workspace *oth_ws;

	if (strcasecmp(argv[2], "number") == 0) {
		if (!isdigit(argv[3][0])) {
			return cmd_results_new(CMD_INVALID,
					"Invalid workspace number '%s'", argv[3]);
		}

		ws_name = join_args(argv + 3, argc - 3);
		oth_ws = workspace_by_number(ws_name);
	} else {
		ws_name = join_args(argv + 2, argc - 2);
		oth_ws = workspace_by_name(ws_name);
	}

	if (!oth_ws) {
		oth_ws = workspace_create(NULL, ws_name);

		if (!oth_ws) {
		    return cmd_results_new(CMD_FAILURE,
					"Unable to create new workspace");
		}
	}

	free(ws_name);

	// second workspace is the one currently focused
	struct sway_workspace *cur_ws = config->handler_context.workspace;
	if (!cur_ws) {
		return cmd_results_new(CMD_FAILURE, NULL);
	}

	// exit early if there is nothing to swap
	if (cur_ws == oth_ws) return cmd_results_new(CMD_SUCCESS, NULL);

	// save seat to set the focus later
	struct sway_seat *seat = config->handler_context.seat;
	swap_workspace_properties(cur_ws, oth_ws);

	node_set_dirty(&cur_ws->node);
	node_set_dirty(&oth_ws->node);
	workspace_update_representation(cur_ws);
	workspace_update_representation(oth_ws);

	// before rearranging the workspaces we have to set a few properties
	// such as dirty
	workspace_for_each_container(cur_ws, handle_container_after_move, cur_ws);
	workspace_for_each_container(oth_ws, handle_container_after_move, oth_ws);

	workspace_detect_urgent(cur_ws);
	workspace_detect_urgent(oth_ws);

	// after swapping we set the focus on the first container in the current
	// workspace or the workspace itself if there is no container
	set_new_focus(cur_ws, seat);

	// destroy other workspace in case it is empty
	workspace_consider_destroy(oth_ws);

	// update both affected workspaces
	arrange_workspace(cur_ws);
	arrange_workspace(oth_ws);

	return cmd_results_new(CMD_SUCCESS, NULL);
}

static bool test_con_id(struct sway_container *container, void *data) {
	size_t *con_id = data;
	return container->node.id == *con_id;
}

#if HAVE_XWAYLAND
static bool test_id(struct sway_container *container, void *data) {
	xcb_window_t *wid = data;
	return (container->view && container->view->type == SWAY_VIEW_XWAYLAND
			&& container->view->wlr_xwayland_surface->window_id == *wid);
}
#endif

static bool test_mark(struct sway_container *container, void *mark) {
	if (container->marks->length) {
		return list_seq_find(container->marks,
				(int (*)(const void *, const void *))strcmp, mark) != -1;
	}
	return false;
}

static struct cmd_results *swap_containers(int argc, char **argv) {
	struct cmd_results *error = NULL;

	struct sway_container *current = config->handler_context.container;
	struct sway_container *other = NULL;

	char *value = join_args(argv + 3, argc - 3);
	if (strcasecmp(argv[2], "id") == 0) {
#if HAVE_XWAYLAND
		xcb_window_t id = strtol(value, NULL, 0);
		other = root_find_container(test_id, &id);
#endif
	} else if (strcasecmp(argv[2], "con_id") == 0) {
		size_t con_id = atoi(value);
		other = root_find_container(test_con_id, &con_id);
	} else if (strcasecmp(argv[2], "mark") == 0) {
		other = root_find_container(test_mark, value);
	} else {
		free(value);
		return cmd_results_new(CMD_INVALID, "%s", expected_syntax);
	}

	if (!other) {
		error = cmd_results_new(CMD_FAILURE,
				"Failed to find %s '%s'", argv[2], value);
	} else if (!current) {
		error = cmd_results_new(CMD_FAILURE,
				"Can only swap with containers and views");
	} else if (current == other) {
		error = cmd_results_new(CMD_FAILURE,
				"Cannot swap a container with itself");
	} else if (container_has_ancestor(current, other)
			|| container_has_ancestor(other, current)) {
		error = cmd_results_new(CMD_FAILURE,
				"Cannot swap ancestor and descendant");
	}

	free(value);

	if (error) {
		return error;
	}

	container_swap(current, other);

	if (root->fullscreen_global) {
		arrange_root();
	} else {
		struct sway_node *current_parent = node_get_parent(&current->node);
		struct sway_node *other_parent = node_get_parent(&other->node);
		if (current_parent) {
			arrange_node(current_parent);
		}
		if (other_parent && current_parent != other_parent) {
			arrange_node(other_parent);
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_swap(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "swap", EXPECTED_AT_LEAST, 4))) {
		return error;
	}
	if (!root->outputs->length) {
		return cmd_results_new(CMD_INVALID,
				"Can't run this command while there's no outputs connected.");
	}

	if (strcasecmp(argv[0], "container") == 0 && strcasecmp(argv[1], "with") == 0) {
		return swap_containers(argc, argv);
	} else if (strcasecmp(argv[0], "workspace") == 0 && strcasecmp(argv[1], "with") == 0) {
		return swap_workspaces(argc, argv);
	}

	return cmd_results_new(CMD_INVALID, "%s", expected_syntax);
}
