#include "log.h"
#include "stringop.h"
#include "sway/output.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "sway/commands.h"
#include "sway/ipc-server.h"
#include "stringop.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>


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
		struct sway_container *first_view;
		struct sway_container *container = ws->tiling->items[0];

		container_get_first_view(container, &first_view);
		seat_set_focus(seat, &first_view->node);
	} else if (ws->floating->length) {
		seat_set_focus(seat, ws->floating->items[0]);
	} else {
		seat_set_focus(seat, &ws->node);
	}
}

/*
 * swap the contents of the currently focused workspace with the content
 * of the workspace specified in the args
 *
 * syntax: swap_workspace with <name|number x>
 */

struct cmd_results *cmd_swap_workspace_content(int argc, char **argv) {
	// parse arguments
	if (argc < 2) {
		return cmd_results_new(CMD_INVALID, "syntax not supported");
	}

	if (strcasecmp(argv[0], "with") != 0) {
		return cmd_results_new(CMD_FAILURE, "Invalid syntax");
	}

	char *ws_name = NULL;
	struct sway_workspace *oth_ws;

	if (strcasecmp(argv[1], "number") == 0) {
		if (!isdigit(argv[2][0])) {
			return cmd_results_new(CMD_INVALID,
					"Invalid workspace number '%s'", argv[2]);
		}

		ws_name = join_args(argv + 2, argc - 2);
		oth_ws = workspace_by_number(ws_name);
	} else {
		ws_name = join_args(argv + 1, argc - 1);
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
