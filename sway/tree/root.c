#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output_layout.h>
#include "sway/desktop/transaction.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/workspace.h"
#include "list.h"
#include "log.h"
#include "util.h"

struct sway_root *root;

static void output_layout_handle_change(struct wl_listener *listener,
		void *data) {
	arrange_root();
	transaction_commit_dirty();
}

struct sway_root *root_create(void) {
	struct sway_root *root = calloc(1, sizeof(struct sway_root));
	if (!root) {
		sway_log(SWAY_ERROR, "Unable to allocate sway_root");
		return NULL;
	}
	node_init(&root->node, N_ROOT, root);
	root->output_layout = wlr_output_layout_create();
	wl_list_init(&root->all_outputs);
#if HAVE_XWAYLAND
	wl_list_init(&root->xwayland_unmanaged);
#endif
	wl_list_init(&root->drag_icons);
	wl_signal_init(&root->events.new_node);
	root->outputs = create_list();
	root->non_desktop_outputs = create_list();
	root->scratchpad = create_list();

	root->output_layout_change.notify = output_layout_handle_change;
	wl_signal_add(&root->output_layout->events.change,
		&root->output_layout_change);
	return root;
}

void root_destroy(struct sway_root *root) {
	wl_list_remove(&root->output_layout_change.link);
	list_free(root->scratchpad);
	list_free(root->non_desktop_outputs);
	list_free(root->outputs);
	wlr_output_layout_destroy(root->output_layout);
	free(root);
}

void root_scratchpad_add_container(struct sway_container *con, struct sway_workspace *ws) {
	if (!sway_assert(!con->scratchpad, "Container is already in scratchpad")) {
		return;
	}

	struct sway_container *parent = con->pending.parent;
	struct sway_workspace *workspace = con->pending.workspace;

	// Clear the fullscreen mode when sending to the scratchpad
	if (con->pending.fullscreen_mode != FULLSCREEN_NONE) {
		container_fullscreen_disable(con);
	}

	// When a tiled window is sent to scratchpad, center and resize it.
	if (!container_is_floating(con)) {
		container_set_floating(con, true);
		container_floating_set_default_size(con);
		container_floating_move_to_center(con);
	}

	container_detach(con);
	con->scratchpad = true;
	list_add(root->scratchpad, con);
	if (ws) {
		workspace_add_floating(ws, con);
	}

	if (!ws) {
		struct sway_seat *seat = input_manager_current_seat();
		struct sway_node *new_focus = NULL;
		if (parent) {
			arrange_container(parent);
			new_focus = seat_get_focus_inactive(seat, &parent->node);
		}
		if (!new_focus) {
			arrange_workspace(workspace);
			new_focus = seat_get_focus_inactive(seat, &workspace->node);
		}
		seat_set_focus(seat, new_focus);
	}

	ipc_event_window(con, "move");
}

void root_scratchpad_remove_container(struct sway_container *con) {
	if (!sway_assert(con->scratchpad, "Container is not in scratchpad")) {
		return;
	}
	con->scratchpad = false;
	int index = list_find(root->scratchpad, con);
	if (index != -1) {
		list_del(root->scratchpad, index);
		ipc_event_window(con, "move");
	}
}

void root_scratchpad_show(struct sway_container *con) {
	struct sway_seat *seat = input_manager_current_seat();
	struct sway_workspace *new_ws = seat_get_focused_workspace(seat);
	if (!new_ws) {
		sway_log(SWAY_DEBUG, "No focused workspace to show scratchpad on");
		return;
	}
	struct sway_workspace *old_ws = con->pending.workspace;

	// If the current con or any of its parents are in fullscreen mode, we
	// first need to disable it before showing the scratchpad con.
	if (new_ws->fullscreen) {
		container_fullscreen_disable(new_ws->fullscreen);
	}
	if (root->fullscreen_global) {
		container_fullscreen_disable(root->fullscreen_global);
	}

	// Show the container
	if (old_ws) {
		container_detach(con);
		workspace_consider_destroy(old_ws);
	} else {
		// Act on the ancestor of scratchpad hidden split containers
		while (con->pending.parent) {
			con = con->pending.parent;
		}
	}
	workspace_add_floating(new_ws, con);

	// Make sure the container's center point overlaps this workspace
	double center_lx = con->pending.x + con->pending.width / 2;
	double center_ly = con->pending.y + con->pending.height / 2;

	struct wlr_box workspace_box;
	workspace_get_box(new_ws, &workspace_box);
	if (!wlr_box_contains_point(&workspace_box, center_lx, center_ly)) {
		container_floating_resize_and_center(con);
	}

	arrange_workspace(new_ws);
	seat_set_focus(seat, seat_get_focus_inactive(seat, &con->node));
}

static void disable_fullscreen(struct sway_container *con, void *data) {
	if (con->pending.fullscreen_mode != FULLSCREEN_NONE) {
		container_fullscreen_disable(con);
	}
}

void root_scratchpad_hide(struct sway_container *con) {
	struct sway_seat *seat = input_manager_current_seat();
	struct sway_node *focus = seat_get_focus_inactive(seat, &root->node);
	struct sway_workspace *ws = con->pending.workspace;

	if (con->pending.fullscreen_mode == FULLSCREEN_GLOBAL && !con->pending.workspace) {
		// If the container was made fullscreen global while in the scratchpad,
		// it should be shown until fullscreen has been disabled
		return;
	}

	disable_fullscreen(con, NULL);
	container_for_each_child(con, disable_fullscreen, NULL);
	container_detach(con);
	arrange_workspace(ws);
	if (&con->node == focus || node_has_ancestor(focus, &con->node)) {
		seat_set_focus(seat, seat_get_focus_inactive(seat, &ws->node));
	}
	list_move_to_end(root->scratchpad, con);

	ipc_event_window(con, "move");
}

void root_for_each_workspace(void (*f)(struct sway_workspace *ws, void *data),
		void *data) {
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		output_for_each_workspace(output, f, data);
	}
}

void root_for_each_container(void (*f)(struct sway_container *con, void *data),
		void *data) {
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		output_for_each_container(output, f, data);
	}

	// Scratchpad
	for (int i = 0; i < root->scratchpad->length; ++i) {
		struct sway_container *container = root->scratchpad->items[i];
		if (container_is_scratchpad_hidden(container)) {
			f(container, data);
			container_for_each_child(container, f, data);
		}
	}

	// Saved workspaces
	for (int i = 0; i < root->fallback_output->workspaces->length; ++i) {
		struct sway_workspace *ws = root->fallback_output->workspaces->items[i];
		workspace_for_each_container(ws, f, data);
	}
}

struct sway_output *root_find_output(
		bool (*test)(struct sway_output *output, void *data), void *data) {
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		if (test(output, data)) {
			return output;
		}
	}
	return NULL;
}

struct sway_workspace *root_find_workspace(
		bool (*test)(struct sway_workspace *ws, void *data), void *data) {
	struct sway_workspace *result = NULL;
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		if ((result = output_find_workspace(output, test, data))) {
			return result;
		}
	}
	return NULL;
}

struct sway_container *root_find_container(
		bool (*test)(struct sway_container *con, void *data), void *data) {
	struct sway_container *result = NULL;
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		if ((result = output_find_container(output, test, data))) {
			return result;
		}
	}

	// Scratchpad
	for (int i = 0; i < root->scratchpad->length; ++i) {
		struct sway_container *container = root->scratchpad->items[i];
		if (container_is_scratchpad_hidden(container)) {
			if (test(container, data)) {
				return container;
			}
			if ((result = container_find_child(container, test, data))) {
				return result;
			}
		}
	}

	// Saved workspaces
	for (int i = 0; i < root->fallback_output->workspaces->length; ++i) {
		struct sway_workspace *ws = root->fallback_output->workspaces->items[i];
		if ((result = workspace_find_container(ws, test, data))) {
			return result;
		}
	}

	return NULL;
}

void root_get_box(struct sway_root *root, struct wlr_box *box) {
	box->x = root->x;
	box->y = root->y;
	box->width = root->width;
	box->height = root->height;
}
