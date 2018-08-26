#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include "config.h"
#include "sway/debug.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/output.h"
#include "sway/tree/workspace.h"
#include "sway/tree/view.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "list.h"
#include "log.h"

void container_handle_fullscreen_reparent(struct sway_container *con,
		struct sway_container *old_parent) {
	if (!con->is_fullscreen) {
		return;
	}
	struct sway_container *old_workspace = old_parent;
	if (old_workspace && old_workspace->type != C_WORKSPACE) {
		old_workspace = container_parent(old_workspace, C_WORKSPACE);
	}
	struct sway_container *new_workspace = container_parent(con, C_WORKSPACE);
	if (old_workspace == new_workspace) {
		return;
	}
	// Unmark the old workspace as fullscreen
	if (old_workspace) {
		old_workspace->sway_workspace->fullscreen = NULL;
	}

	// Mark the new workspace as fullscreen
	if (new_workspace->sway_workspace->fullscreen) {
		container_set_fullscreen(
				new_workspace->sway_workspace->fullscreen, false);
	}
	new_workspace->sway_workspace->fullscreen = con;

	// Resize container to new output dimensions
	struct sway_container *output = new_workspace->parent;
	con->x = output->x;
	con->y = output->y;
	con->width = output->width;
	con->height = output->height;

	if (con->type == C_VIEW) {
		struct sway_view *view = con->sway_view;
		view->x = output->x;
		view->y = output->y;
		view->width = output->width;
		view->height = output->height;
	} else {
		arrange_windows(new_workspace);
	}
}

void container_insert_child(struct sway_container *parent,
		struct sway_container *child, int i) {
	struct sway_container *old_parent = child->parent;
	if (old_parent) {
		container_remove_child(child);
	}
	wlr_log(WLR_DEBUG, "Inserting id:%zd at index %d", child->id, i);
	list_insert(parent->children, i, child);
	child->parent = parent;
	container_handle_fullscreen_reparent(child, old_parent);
}

struct sway_container *container_add_sibling(struct sway_container *fixed,
		struct sway_container *active) {
	// TODO handle floating
	struct sway_container *old_parent = NULL;
	if (active->parent) {
		old_parent = active->parent;
		container_remove_child(active);
	}
	struct sway_container *parent = fixed->parent;
	int i = container_sibling_index(fixed);
	list_insert(parent->children, i + 1, active);
	active->parent = parent;
	container_handle_fullscreen_reparent(active, old_parent);
	return active->parent;
}

void container_add_child(struct sway_container *parent,
		struct sway_container *child) {
	wlr_log(WLR_DEBUG, "Adding %p (%d, %fx%f) to %p (%d, %fx%f)",
			child, child->type, child->width, child->height,
			parent, parent->type, parent->width, parent->height);
	struct sway_container *old_parent = child->parent;
	list_add(parent->children, child);
	child->parent = parent;
	container_handle_fullscreen_reparent(child, old_parent);
	if (old_parent) {
		container_set_dirty(old_parent);
	}
	container_set_dirty(child);
}

struct sway_container *container_remove_child(struct sway_container *child) {
	if (child->is_fullscreen) {
		struct sway_container *workspace = container_parent(child, C_WORKSPACE);
		workspace->sway_workspace->fullscreen = NULL;
	}

	struct sway_container *parent = child->parent;
	list_t *list = container_is_floating(child) ?
		parent->sway_workspace->floating : parent->children;
	int index = list_find(list, child);
	if (index != -1) {
		list_del(list, index);
	}
	child->parent = NULL;
	container_notify_subtree_changed(parent);

	container_set_dirty(parent);
	container_set_dirty(child);

	return parent;
}

bool sway_dir_to_wlr(enum movement_direction dir, enum wlr_direction *out) {
	switch (dir) {
	case MOVE_UP:
		*out = WLR_DIRECTION_UP;
		break;
	case MOVE_DOWN:
		*out = WLR_DIRECTION_DOWN;
		break;
	case MOVE_LEFT:
		*out = WLR_DIRECTION_LEFT;
		break;
	case MOVE_RIGHT:
		*out = WLR_DIRECTION_RIGHT;
		break;
	default:
		return false;
	}

	return true;
}

enum sway_container_layout container_get_default_layout(
		struct sway_container *con) {
	if (con->type != C_OUTPUT) {
		con = container_parent(con, C_OUTPUT);
	}

	if (!sway_assert(con != NULL,
			"container_get_default_layout must be called on an attached"
			" container below the root container")) {
		return 0;
	}

	if (config->default_layout != L_NONE) {
		return config->default_layout;
	} else if (config->default_orientation != L_NONE) {
		return config->default_orientation;
	} else if (con->width >= con->height) {
		return L_HORIZ;
	} else {
		return L_VERT;
	}
}

struct sway_container *container_replace_child(struct sway_container *child,
		struct sway_container *new_child) {
	struct sway_container *parent = child->parent;
	if (parent == NULL) {
		return NULL;
	}

	list_t *list = container_is_floating(child) ?
		parent->sway_workspace->floating : parent->children;
	int i = list_find(list, child);

	if (new_child->parent) {
		container_remove_child(new_child);
	}
	list->items[i] = new_child;
	new_child->parent = parent;
	child->parent = NULL;

	// Set geometry for new child
	new_child->x = child->x;
	new_child->y = child->y;
	new_child->width = child->width;
	new_child->height = child->height;

	// reset geometry for child
	child->width = 0;
	child->height = 0;

	return parent;
}

struct sway_container *container_split(struct sway_container *child,
		enum sway_container_layout layout) {
	// TODO floating: cannot split a floating container
	if (!sway_assert(child, "child cannot be null")) {
		return NULL;
	}
	if (child->type == C_WORKSPACE && child->children->length == 0) {
		// Special case: this just behaves like splitt
		child->prev_split_layout = child->layout;
		child->layout = layout;
		return child;
	}

	struct sway_container *cont = container_create(C_CONTAINER);

	wlr_log(WLR_DEBUG, "creating container %p around %p", cont, child);

	child->type == C_WORKSPACE ? workspace_remove_gaps(child)
		: container_remove_gaps(child);

	cont->prev_split_layout = L_NONE;
	cont->width = child->width;
	cont->height = child->height;
	cont->x = child->x;
	cont->y = child->y;

	struct sway_seat *seat = input_manager_get_default_seat(input_manager);
	bool set_focus = (seat_get_focus(seat) == child);

	container_add_gaps(cont);

	if (child->type == C_WORKSPACE) {
		struct sway_container *workspace = child;
		while (workspace->children->length) {
			struct sway_container *ws_child = workspace->children->items[0];
			container_remove_child(ws_child);
			container_add_child(cont, ws_child);
		}

		container_add_child(workspace, cont);
		enum sway_container_layout old_layout = workspace->layout;
		workspace->layout = layout;
		cont->layout = old_layout;
	} else {
		cont->layout = layout;
		container_replace_child(child, cont);
		container_add_child(cont, child);
	}

	if (set_focus) {
		seat_set_focus(seat, cont);
		seat_set_focus(seat, child);
	}

	container_notify_subtree_changed(cont);
	return cont;
}

void container_recursive_resize(struct sway_container *container,
		double amount, enum wlr_edges edge) {
	bool layout_match = true;
	wlr_log(WLR_DEBUG, "Resizing %p with amount: %f", container, amount);
	if (edge == WLR_EDGE_LEFT || edge == WLR_EDGE_RIGHT) {
		container->width += amount;
		layout_match = container->layout == L_HORIZ;
	} else if (edge == WLR_EDGE_TOP || edge == WLR_EDGE_BOTTOM) {
		container->height += amount;
		layout_match = container->layout == L_VERT;
	}
	if (container->children) {
		for (int i = 0; i < container->children->length; i++) {
			struct sway_container *child = container->children->items[i];
			double amt = layout_match ?
				amount / container->children->length : amount;
			container_recursive_resize(child, amt, edge);
		}
	}
}

static void swap_places(struct sway_container *con1,
		struct sway_container *con2) {
	struct sway_container *temp = malloc(sizeof(struct sway_container));
	temp->x = con1->x;
	temp->y = con1->y;
	temp->width = con1->width;
	temp->height = con1->height;
	temp->parent = con1->parent;

	con1->x = con2->x;
	con1->y = con2->y;
	con1->width = con2->width;
	con1->height = con2->height;

	con2->x = temp->x;
	con2->y = temp->y;
	con2->width = temp->width;
	con2->height = temp->height;

	int temp_index = container_sibling_index(con1);
	container_insert_child(con2->parent, con1, container_sibling_index(con2));
	container_insert_child(temp->parent, con2, temp_index);

	free(temp);
}

static void swap_focus(struct sway_container *con1,
		struct sway_container *con2, struct sway_seat *seat,
		struct sway_container *focus) {
	if (focus == con1 || focus == con2) {
		struct sway_container *ws1 = container_parent(con1, C_WORKSPACE);
		struct sway_container *ws2 = container_parent(con2, C_WORKSPACE);
		if (focus == con1 && (con2->parent->layout == L_TABBED
					|| con2->parent->layout == L_STACKED)) {
			if (workspace_is_visible(ws2)) {
				seat_set_focus_warp(seat, con2, false, true);
			}
			seat_set_focus(seat, ws1 != ws2 ? con2 : con1);
		} else if (focus == con2 && (con1->parent->layout == L_TABBED
					|| con1->parent->layout == L_STACKED)) {
			if (workspace_is_visible(ws1)) {
				seat_set_focus_warp(seat, con1, false, true);
			}
			seat_set_focus(seat, ws1 != ws2 ? con1 : con2);
		} else if (ws1 != ws2) {
			seat_set_focus(seat, focus == con1 ? con2 : con1);
		} else {
			seat_set_focus(seat, focus);
		}
	} else {
		seat_set_focus(seat, focus);
	}
}

void container_swap(struct sway_container *con1, struct sway_container *con2) {
	if (!sway_assert(con1 && con2, "Cannot swap with nothing")) {
		return;
	}
	if (!sway_assert(con1->type >= C_CONTAINER && con2->type >= C_CONTAINER,
				"Can only swap containers and views")) {
		return;
	}
	if (!sway_assert(!container_has_ancestor(con1, con2)
				&& !container_has_ancestor(con2, con1),
				"Cannot swap ancestor and descendant")) {
		return;
	}
	if (!sway_assert(!container_is_floating(con1)
				&& !container_is_floating(con2),
				"Swapping with floating containers is not supported")) {
		return;
	}

	wlr_log(WLR_DEBUG, "Swapping containers %zu and %zu", con1->id, con2->id);

	int fs1 = con1->is_fullscreen;
	int fs2 = con2->is_fullscreen;
	if (fs1) {
		container_set_fullscreen(con1, false);
	}
	if (fs2) {
		container_set_fullscreen(con2, false);
	}

	struct sway_seat *seat = input_manager_get_default_seat(input_manager);
	struct sway_container *focus = seat_get_focus(seat);
	struct sway_container *vis1 = container_parent(
			seat_get_focus_inactive(seat, container_parent(con1, C_OUTPUT)),
			C_WORKSPACE);
	struct sway_container *vis2 = container_parent(
			seat_get_focus_inactive(seat, container_parent(con2, C_OUTPUT)),
			C_WORKSPACE);

	char *stored_prev_name = NULL;
	if (prev_workspace_name) {
		stored_prev_name = strdup(prev_workspace_name);
	}

	swap_places(con1, con2);

	if (!workspace_is_visible(vis1)) {
		seat_set_focus(seat, seat_get_focus_inactive(seat, vis1));
	}
	if (!workspace_is_visible(vis2)) {
		seat_set_focus(seat, seat_get_focus_inactive(seat, vis2));
	}

	swap_focus(con1, con2, seat, focus);

	if (stored_prev_name) {
		free(prev_workspace_name);
		prev_workspace_name = stored_prev_name;
	}

	if (fs1) {
		container_set_fullscreen(con2, true);
	}
	if (fs2) {
		container_set_fullscreen(con1, true);
	}
}
