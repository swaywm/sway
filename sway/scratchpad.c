#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "sway/scratchpad.h"
#include "sway/input/seat.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "list.h"
#include "log.h"

void scratchpad_add_container(struct sway_container *con) {
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

void scratchpad_remove_container(struct sway_container *con) {
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

/**
 * Show a single scratchpad container.
 * The container might be visible on another workspace already.
 */
static void scratchpad_show(struct sway_container *con) {
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

/**
 * Hide a single scratchpad container.
 * The container might not be the focused container (eg. when using criteria).
 */
static void scratchpad_hide(struct sway_container *con) {
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

void scratchpad_toggle_auto(void) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focus(seat);
	struct sway_container *ws = focus->type == C_WORKSPACE ?
		focus : container_parent(focus, C_WORKSPACE);

	// If the focus is in a floating split container,
	// operate on the split container instead of the child.
	if (container_is_floating_or_child(focus)) {
		while (focus->parent->layout != L_FLOATING) {
			focus = focus->parent;
		}
	}


    // Check if the currently focused window is a scratchpad window and should
    // be hidden again.
	if (focus->scratchpad) {
		wlr_log(WLR_DEBUG, "Focus is a scratchpad window - hiding %s",
				focus->name);
		scratchpad_hide(focus);
		return;
	}

    // Check if there is an unfocused scratchpad window on the current workspace
    // and focus it.
	for (int i = 0; i < ws->sway_workspace->floating->children->length; ++i) {
		struct sway_container *floater =
			ws->sway_workspace->floating->children->items[i];
		if (floater->scratchpad && focus != floater) {
			wlr_log(WLR_DEBUG,
					"Focusing other scratchpad window (%s) in this workspace",
					floater->name);
			scratchpad_show(floater);
			return;
		}
	}

    // Check if there is a visible scratchpad window on another workspace.
    // In this case we move it to the current workspace.
	for (int i = 0; i < root_container.sway_root->scratchpad->length; ++i) {
		struct sway_container *con =
			root_container.sway_root->scratchpad->items[i];
		if (con->parent) {
			wlr_log(WLR_DEBUG,
					"Moving a visible scratchpad window (%s) to this workspace",
					con->name);
			scratchpad_show(con);
			return;
		}
	}

	// Take the container at the bottom of the scratchpad list
	if (!sway_assert(root_container.sway_root->scratchpad->length,
				"Scratchpad is empty")) {
		return;
	}
	struct sway_container *con = root_container.sway_root->scratchpad->items[0];
	wlr_log(WLR_DEBUG, "Showing %s from list", con->name);
	scratchpad_show(con);
}

void scratchpad_toggle_container(struct sway_container *con) {
	if (!sway_assert(con->scratchpad, "Container isn't in the scratchpad")) {
		return;
	}

    // Check if it matches a currently visible scratchpad window and hide it.
	if (con->parent) {
		scratchpad_hide(con);
		return;
	}

	scratchpad_show(con);
}
