#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output_layout.h>
#include "sway/input/seat.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/workspace.h"
#include "list.h"
#include "log.h"

struct sway_container root_container;

static void output_layout_handle_change(struct wl_listener *listener,
		void *data) {
	arrange_windows(&root_container);
	transaction_commit_dirty();
}

void root_create(void) {
	root_container.id = 0; // normally assigned in new_swayc()
	root_container.type = C_ROOT;
	root_container.layout = L_NONE;
	root_container.name = strdup("root");
	root_container.instructions = create_list();
	root_container.children = create_list();
	root_container.current.children = create_list();
	wl_signal_init(&root_container.events.destroy);

	root_container.sway_root = calloc(1, sizeof(*root_container.sway_root));
	root_container.sway_root->output_layout = wlr_output_layout_create();
	wl_list_init(&root_container.sway_root->outputs);
#ifdef HAVE_XWAYLAND
	wl_list_init(&root_container.sway_root->xwayland_unmanaged);
#endif
	wl_list_init(&root_container.sway_root->drag_icons);
	wl_signal_init(&root_container.sway_root->events.new_container);
	root_container.sway_root->scratchpad = create_list();

	root_container.sway_root->output_layout_change.notify =
		output_layout_handle_change;
	wl_signal_add(&root_container.sway_root->output_layout->events.change,
		&root_container.sway_root->output_layout_change);
}

void root_destroy(void) {
	// sway_root
	wl_list_remove(&root_container.sway_root->output_layout_change.link);
	list_free(root_container.sway_root->scratchpad);
	wlr_output_layout_destroy(root_container.sway_root->output_layout);
	free(root_container.sway_root);

	// root_container
	list_free(root_container.instructions);
	list_free(root_container.children);
	list_free(root_container.current.children);
	free(root_container.name);

	memset(&root_container, 0, sizeof(root_container));
}

void root_scratchpad_add_container(struct sway_container *con) {
	if (!sway_assert(!con->scratchpad, "Container is already in scratchpad")) {
		return;
	}
	con->scratchpad = true;
	list_add(root_container.sway_root->scratchpad, con);

	struct sway_container *parent = con->parent;
	container_set_floating(con, true);
	container_remove_child(con);
	arrange_windows(parent);

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	seat_set_focus(seat, seat_get_focus_inactive(seat, parent));
}

void root_scratchpad_remove_container(struct sway_container *con) {
	if (!sway_assert(con->scratchpad, "Container is not in scratchpad")) {
		return;
	}
	con->scratchpad = false;
	for (int i = 0; i < root_container.sway_root->scratchpad->length; ++i) {
		if (root_container.sway_root->scratchpad->items[i] == con) {
			list_del(root_container.sway_root->scratchpad, i);
			break;
		}
	}
}

void root_scratchpad_show(struct sway_container *con) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *ws = seat_get_focus(seat);
	if (ws->type != C_WORKSPACE) {
		ws = container_parent(ws, C_WORKSPACE);
	}

    // If the current con or any of its parents are in fullscreen mode, we
    // first need to disable it before showing the scratchpad con.
	if (ws->sway_workspace->fullscreen) {
		container_set_fullscreen(ws->sway_workspace->fullscreen, false);
	}

	// Show the container
	if (con->parent) {
		container_remove_child(con);
	}
	container_add_child(ws->sway_workspace->floating, con);

	// Make sure the container's center point overlaps this workspace
	double center_lx = con->x + con->width / 2;
	double center_ly = con->y + con->height / 2;

	struct wlr_box workspace_box;
	container_get_box(ws, &workspace_box);
	if (!wlr_box_contains_point(&workspace_box, center_lx, center_ly)) {
		// Maybe resize it
		if (con->width > ws->width || con->height > ws->height) {
			container_init_floating(con);
		}

		// Center it
		double new_lx = ws->x + (ws->width - con->width) / 2;
		double new_ly = ws->y + (ws->height - con->height) / 2;
		container_floating_move_to(con, new_lx, new_ly);
	}

	arrange_windows(ws);
	seat_set_focus(seat, seat_get_focus_inactive(seat, con));

	container_set_dirty(con->parent);
}

void root_scratchpad_hide(struct sway_container *con) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focus(seat);
	struct sway_container *ws = container_parent(con, C_WORKSPACE);

	container_remove_child(con);
	arrange_windows(ws);
	if (con == focus) {
		seat_set_focus(seat, seat_get_focus_inactive(seat, ws));
	}
	list_move_to_end(root_container.sway_root->scratchpad, con);
}
