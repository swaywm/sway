#define _XOPEN_SOURCE 700
#include <math.h>
#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif
#include <limits.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_idle.h>
#include "list.h"
#include "log.h"
#include "config.h"
#include "sway/commands.h"
#include "sway/desktop.h"
#include "sway/desktop/transaction.h"
#include "sway/input/cursor.h"
#include "sway/input/keyboard.h"
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"

// When doing a tiling drag, this is the thickness of the dropzone
// when dragging to the edge of a layout container.
#define DROP_LAYOUT_BORDER 30

static uint32_t get_current_time_msec() {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return now.tv_nsec / 1000;
}

static struct wlr_surface *layer_surface_at(struct sway_output *output,
		struct wl_list *layer, double ox, double oy, double *sx, double *sy) {
	struct sway_layer_surface *sway_layer;
	wl_list_for_each_reverse(sway_layer, layer, link) {
		struct wlr_surface *wlr_surface =
			sway_layer->layer_surface->surface;
		double _sx = ox - sway_layer->geo.x;
		double _sy = oy - sway_layer->geo.y;
		// TODO: Test popups/subsurfaces
		if (wlr_surface_point_accepts_input(wlr_surface, _sx, _sy)) {
			*sx = _sx;
			*sy = _sy;
			return wlr_surface;
		}
	}
	return NULL;
}

/**
 * Returns the node at the cursor's position. If there is a surface at that
 * location, it is stored in **surface (it may not be a view).
 */
static struct sway_node *node_at_coords(
		struct sway_seat *seat, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	// check for unmanaged views first
#ifdef HAVE_XWAYLAND
	struct wl_list *unmanaged = &root->xwayland_unmanaged;
	struct sway_xwayland_unmanaged *unmanaged_surface;
	wl_list_for_each_reverse(unmanaged_surface, unmanaged, link) {
		struct wlr_xwayland_surface *xsurface =
			unmanaged_surface->wlr_xwayland_surface;

		double _sx = lx - unmanaged_surface->lx;
		double _sy = ly - unmanaged_surface->ly;
		if (wlr_surface_point_accepts_input(xsurface->surface, _sx, _sy)) {
			*surface = xsurface->surface;
			*sx = _sx;
			*sy = _sy;
			return NULL;
		}
	}
#endif
	// find the output the cursor is on
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
			root->output_layout, lx, ly);
	if (wlr_output == NULL) {
		return NULL;
	}
	struct sway_output *output = wlr_output->data;
	double ox = lx, oy = ly;
	wlr_output_layout_output_coords(root->output_layout, wlr_output, &ox, &oy);

	// find the focused workspace on the output for this seat
	struct sway_workspace *ws = output_get_active_workspace(output);

	if ((*surface = layer_surface_at(output,
				&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],
				ox, oy, sx, sy))) {
		return NULL;
	}
	if (ws->fullscreen) {
		struct sway_container *con =
			tiling_container_at(&ws->fullscreen->node, lx, ly, surface, sx, sy);
		if (con) {
			return &con->node;
		}
		return NULL;
	}
	if ((*surface = layer_surface_at(output,
				&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP],
				ox, oy, sx, sy))) {
		return NULL;
	}

	struct sway_container *c;
	if ((c = container_at(ws, lx, ly, surface, sx, sy))) {
		return &c->node;
	}

	if ((*surface = layer_surface_at(output,
				&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM],
				ox, oy, sx, sy))) {
		return NULL;
	}
	if ((*surface = layer_surface_at(output,
				&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND],
				ox, oy, sx, sy))) {
		return NULL;
	}

	return &ws->node;
}

/**
 * Determine if the edge of the given container is on the edge of the
 * workspace/output.
 */
static bool edge_is_external(struct sway_container *cont, enum wlr_edges edge) {
	enum sway_container_layout layout = L_NONE;
	switch (edge) {
	case WLR_EDGE_TOP:
	case WLR_EDGE_BOTTOM:
		layout = L_VERT;
		break;
	case WLR_EDGE_LEFT:
	case WLR_EDGE_RIGHT:
		layout = L_HORIZ;
		break;
	case WLR_EDGE_NONE:
		sway_assert(false, "Never reached");
		return false;
	}

	// Iterate the parents until we find one with the layout we want,
	// then check if the child has siblings between it and the edge.
	while (cont) {
		if (container_parent_layout(cont) == layout) {
			list_t *siblings = container_get_siblings(cont);
			int index = list_find(siblings, cont);
			if (index > 0 && (edge == WLR_EDGE_LEFT || edge == WLR_EDGE_TOP)) {
				return false;
			}
			if (index < siblings->length - 1 &&
					(edge == WLR_EDGE_RIGHT || edge == WLR_EDGE_BOTTOM)) {
				return false;
			}
		}
		cont = cont->parent;
	}
	return true;
}

static enum wlr_edges find_edge(struct sway_container *cont,
		struct sway_cursor *cursor) {
	if (!cont->view) {
		return WLR_EDGE_NONE;
	}
	struct sway_view *view = cont->view;
	if (view->border == B_NONE || !view->border_thickness || view->using_csd) {
		return WLR_EDGE_NONE;
	}

	enum wlr_edges edge = 0;
	if (cursor->cursor->x < cont->x + view->border_thickness) {
		edge |= WLR_EDGE_LEFT;
	}
	if (cursor->cursor->y < cont->y + view->border_thickness) {
		edge |= WLR_EDGE_TOP;
	}
	if (cursor->cursor->x >= cont->x + cont->width - view->border_thickness) {
		edge |= WLR_EDGE_RIGHT;
	}
	if (cursor->cursor->y >= cont->y + cont->height - view->border_thickness) {
		edge |= WLR_EDGE_BOTTOM;
	}

	return edge;
}

/**
 * If the cursor is over a _resizable_ edge, return the edge.
 * Edges that can't be resized are edges of the workspace.
 */
static enum wlr_edges find_resize_edge(struct sway_container *cont,
		struct sway_cursor *cursor) {
	enum wlr_edges edge = find_edge(cont, cursor);
	if (edge && !container_is_floating(cont) && edge_is_external(cont, edge)) {
		return WLR_EDGE_NONE;
	}
	return edge;
}

static void handle_down_motion(struct sway_seat *seat,
		struct sway_cursor *cursor, uint32_t time_msec) {
	struct sway_container *con = seat->op_container;
	if (seat_is_input_allowed(seat, con->view->surface)) {
		double moved_x = cursor->cursor->x - seat->op_ref_lx;
		double moved_y = cursor->cursor->y - seat->op_ref_ly;
		double sx = seat->op_ref_con_lx + moved_x;
		double sy = seat->op_ref_con_ly + moved_y;
		wlr_seat_pointer_notify_motion(seat->wlr_seat, time_msec, sx, sy);
	}
	seat->op_moved = true;
}

static void handle_move_floating_motion(struct sway_seat *seat,
		struct sway_cursor *cursor) {
	struct sway_container *con = seat->op_container;
	desktop_damage_whole_container(con);
	container_floating_translate(con,
			cursor->cursor->x - cursor->previous.x,
			cursor->cursor->y - cursor->previous.y);
	desktop_damage_whole_container(con);
}

static void resize_box(struct wlr_box *box, enum wlr_edges edge,
		int thickness) {
	switch (edge) {
	case WLR_EDGE_TOP:
		box->height = thickness;
		break;
	case WLR_EDGE_LEFT:
		box->width = thickness;
		break;
	case WLR_EDGE_RIGHT:
		box->x = box->x + box->width - thickness;
		box->width = thickness;
		break;
	case WLR_EDGE_BOTTOM:
		box->y = box->y + box->height - thickness;
		box->height = thickness;
		break;
	case WLR_EDGE_NONE:
		box->x += thickness;
		box->y += thickness;
		box->width -= thickness * 2;
		box->height -= thickness * 2;
		break;
	}
}

static void handle_move_tiling_motion(struct sway_seat *seat,
		struct sway_cursor *cursor) {
	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct sway_node *node = node_at_coords(seat,
			cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);
	// Damage the old location
	desktop_damage_box(&seat->op_drop_box);

	if (!node) {
		// Eg. hovered over a layer surface such as swaybar
		seat->op_target_node = NULL;
		seat->op_target_edge = WLR_EDGE_NONE;
		return;
	}

	if (node->type == N_WORKSPACE) {
		// Emtpy workspace
		seat->op_target_node = node;
		seat->op_target_edge = WLR_EDGE_NONE;
		workspace_get_box(node->sway_workspace, &seat->op_drop_box);
		desktop_damage_box(&seat->op_drop_box);
		return;
	}

	// Deny moving within own workspace if this is the only child
	struct sway_container *con = node->sway_container;
	if (workspace_num_tiling_views(seat->op_container->workspace) == 1 &&
			con->workspace == seat->op_container->workspace) {
		seat->op_target_node = NULL;
		seat->op_target_edge = WLR_EDGE_NONE;
		return;
	}

	// Traverse the ancestors, trying to find a layout container perpendicular
	// to the edge. Eg. close to the top or bottom of a horiz layout.
	while (con) {
		enum wlr_edges edge = WLR_EDGE_NONE;
		enum sway_container_layout layout = container_parent_layout(con);
		struct wlr_box parent;
		con->parent ? container_get_box(con->parent, &parent) :
			workspace_get_box(con->workspace, &parent);
		if (layout == L_HORIZ || layout == L_TABBED) {
			if (cursor->cursor->y < parent.y + DROP_LAYOUT_BORDER) {
				edge = WLR_EDGE_TOP;
			} else if (cursor->cursor->y > parent.y + parent.height
					- DROP_LAYOUT_BORDER) {
				edge = WLR_EDGE_BOTTOM;
			}
		} else if (layout == L_VERT || layout == L_STACKED) {
			if (cursor->cursor->x < parent.x + DROP_LAYOUT_BORDER) {
				edge = WLR_EDGE_LEFT;
			} else if (cursor->cursor->x > parent.x + parent.width
					- DROP_LAYOUT_BORDER) {
				edge = WLR_EDGE_RIGHT;
			}
		}
		if (edge) {
			seat->op_target_node = node_get_parent(&con->node);
			seat->op_target_edge = edge;
			node_get_box(seat->op_target_node, &seat->op_drop_box);
			resize_box(&seat->op_drop_box, edge, DROP_LAYOUT_BORDER);
			desktop_damage_box(&seat->op_drop_box);
			return;
		}
		con = con->parent;
	}

	// Use the hovered view - but we must be over the actual surface
	con = node->sway_container;
	if (!con->view->surface || node == &seat->op_container->node) {
		seat->op_target_node = NULL;
		seat->op_target_edge = WLR_EDGE_NONE;
		return;
	}

	// Find the closest edge
	size_t thickness = fmin(con->view->width, con->view->height) * 0.3;
	size_t closest_dist = INT_MAX;
	size_t dist;
	seat->op_target_edge = WLR_EDGE_NONE;
	if ((dist = cursor->cursor->y - con->y) < closest_dist) {
		closest_dist = dist;
		seat->op_target_edge = WLR_EDGE_TOP;
	}
	if ((dist = cursor->cursor->x - con->x) < closest_dist) {
		closest_dist = dist;
		seat->op_target_edge = WLR_EDGE_LEFT;
	}
	if ((dist = con->x + con->width - cursor->cursor->x) < closest_dist) {
		closest_dist = dist;
		seat->op_target_edge = WLR_EDGE_RIGHT;
	}
	if ((dist = con->y + con->height - cursor->cursor->y) < closest_dist) {
		closest_dist = dist;
		seat->op_target_edge = WLR_EDGE_BOTTOM;
	}

	if (closest_dist > thickness) {
		seat->op_target_edge = WLR_EDGE_NONE;
	}

	seat->op_target_node = node;
	seat->op_drop_box.x = con->view->x;
	seat->op_drop_box.y = con->view->y;
	seat->op_drop_box.width = con->view->width;
	seat->op_drop_box.height = con->view->height;
	resize_box(&seat->op_drop_box, seat->op_target_edge, thickness);
	desktop_damage_box(&seat->op_drop_box);
}

static void calculate_floating_constraints(struct sway_container *con,
		int *min_width, int *max_width, int *min_height, int *max_height) {
	if (config->floating_minimum_width == -1) { // no minimum
		*min_width = 0;
	} else if (config->floating_minimum_width == 0) { // automatic
		*min_width = 75;
	} else {
		*min_width = config->floating_minimum_width;
	}

	if (config->floating_minimum_height == -1) { // no minimum
		*min_height = 0;
	} else if (config->floating_minimum_height == 0) { // automatic
		*min_height = 50;
	} else {
		*min_height = config->floating_minimum_height;
	}

	if (config->floating_maximum_width == -1) { // no maximum
		*max_width = INT_MAX;
	} else if (config->floating_maximum_width == 0) { // automatic
		*max_width = con->workspace->width;
	} else {
		*max_width = config->floating_maximum_width;
	}

	if (config->floating_maximum_height == -1) { // no maximum
		*max_height = INT_MAX;
	} else if (config->floating_maximum_height == 0) { // automatic
		*max_height = con->workspace->height;
	} else {
		*max_height = config->floating_maximum_height;
	}
}

static void handle_resize_floating_motion(struct sway_seat *seat,
		struct sway_cursor *cursor) {
	struct sway_container *con = seat->op_container;
	enum wlr_edges edge = seat->op_resize_edge;

	// The amount the mouse has moved since the start of the resize operation
	// Positive is down/right
	double mouse_move_x = cursor->cursor->x - seat->op_ref_lx;
	double mouse_move_y = cursor->cursor->y - seat->op_ref_ly;

	if (edge == WLR_EDGE_TOP || edge == WLR_EDGE_BOTTOM) {
		mouse_move_x = 0;
	}
	if (edge == WLR_EDGE_LEFT || edge == WLR_EDGE_RIGHT) {
		mouse_move_y = 0;
	}

	double grow_width = edge & WLR_EDGE_LEFT ? -mouse_move_x : mouse_move_x;
	double grow_height = edge & WLR_EDGE_TOP ? -mouse_move_y : mouse_move_y;

	if (seat->op_resize_preserve_ratio) {
		double x_multiplier = grow_width / seat->op_ref_width;
		double y_multiplier = grow_height / seat->op_ref_height;
		double max_multiplier = fmax(x_multiplier, y_multiplier);
		grow_width = seat->op_ref_width * max_multiplier;
		grow_height = seat->op_ref_height * max_multiplier;
	}

	// Determine new width/height, and accommodate for floating min/max values
	double width = seat->op_ref_width + grow_width;
	double height = seat->op_ref_height + grow_height;
	int min_width, max_width, min_height, max_height;
	calculate_floating_constraints(con, &min_width, &max_width,
			&min_height, &max_height);
	width = fmax(min_width, fmin(width, max_width));
	height = fmax(min_height, fmin(height, max_height));

	// Apply the view's min/max size
	if (con->view) {
		double view_min_width, view_max_width, view_min_height, view_max_height;
		view_get_constraints(con->view, &view_min_width, &view_max_width,
				&view_min_height, &view_max_height);
		width = fmax(view_min_width, fmin(width, view_max_width));
		height = fmax(view_min_height, fmin(height, view_max_height));
	}

	// Recalculate these, in case we hit a min/max limit
	grow_width = width - seat->op_ref_width;
	grow_height = height - seat->op_ref_height;

	// Determine grow x/y values - these are relative to the container's x/y at
	// the start of the resize operation.
	double grow_x = 0, grow_y = 0;
	if (edge & WLR_EDGE_LEFT) {
		grow_x = -grow_width;
	} else if (edge & WLR_EDGE_RIGHT) {
		grow_x = 0;
	} else {
		grow_x = -grow_width / 2;
	}
	if (edge & WLR_EDGE_TOP) {
		grow_y = -grow_height;
	} else if (edge & WLR_EDGE_BOTTOM) {
		grow_y = 0;
	} else {
		grow_y = -grow_height / 2;
	}

	// Determine the amounts we need to bump everything relative to the current
	// size.
	int relative_grow_width = width - con->width;
	int relative_grow_height = height - con->height;
	int relative_grow_x = (seat->op_ref_con_lx + grow_x) - con->x;
	int relative_grow_y = (seat->op_ref_con_ly + grow_y) - con->y;

	// Actually resize stuff
	con->x += relative_grow_x;
	con->y += relative_grow_y;
	con->width += relative_grow_width;
	con->height += relative_grow_height;

	if (con->view) {
		struct sway_view *view = con->view;
		view->x += relative_grow_x;
		view->y += relative_grow_y;
		view->width += relative_grow_width;
		view->height += relative_grow_height;
	}

	arrange_container(con);
}

static void handle_resize_tiling_motion(struct sway_seat *seat,
		struct sway_cursor *cursor) {
	int amount_x = 0;
	int amount_y = 0;
	int moved_x = cursor->cursor->x - seat->op_ref_lx;
	int moved_y = cursor->cursor->y - seat->op_ref_ly;
	enum wlr_edges edge_x = WLR_EDGE_NONE;
	enum wlr_edges edge_y = WLR_EDGE_NONE;
	struct sway_container *con = seat->op_container;

	if (seat->op_resize_edge & WLR_EDGE_TOP) {
		amount_y = (seat->op_ref_height - moved_y) - con->height;
		edge_y = WLR_EDGE_TOP;
	} else if (seat->op_resize_edge & WLR_EDGE_BOTTOM) {
		amount_y = (seat->op_ref_height + moved_y) - con->height;
		edge_y = WLR_EDGE_BOTTOM;
	}
	if (seat->op_resize_edge & WLR_EDGE_LEFT) {
		amount_x = (seat->op_ref_width - moved_x) - con->width;
		edge_x = WLR_EDGE_LEFT;
	} else if (seat->op_resize_edge & WLR_EDGE_RIGHT) {
		amount_x = (seat->op_ref_width + moved_x) - con->width;
		edge_x = WLR_EDGE_RIGHT;
	}

	if (amount_x != 0) {
		container_resize_tiled(seat->op_container, edge_x, amount_x);
	}
	if (amount_y != 0) {
		container_resize_tiled(seat->op_container, edge_y, amount_y);
	}
}

void cursor_send_pointer_motion(struct sway_cursor *cursor, uint32_t time_msec,
		bool allow_refocusing) {
	if (time_msec == 0) {
		time_msec = get_current_time_msec();
	}

	struct sway_seat *seat = cursor->seat;
	struct wlr_seat *wlr_seat = seat->wlr_seat;

	if (seat->operation != OP_NONE) {
		switch (seat->operation) {
		case OP_DOWN:
			handle_down_motion(seat, cursor, time_msec);
			break;
		case OP_MOVE_FLOATING:
			handle_move_floating_motion(seat, cursor);
			break;
		case OP_MOVE_TILING:
			handle_move_tiling_motion(seat, cursor);
			break;
		case OP_RESIZE_FLOATING:
			handle_resize_floating_motion(seat, cursor);
			break;
		case OP_RESIZE_TILING:
			handle_resize_tiling_motion(seat, cursor);
			break;
		case OP_NONE:
			break;
		}
		cursor->previous.x = cursor->cursor->x;
		cursor->previous.y = cursor->cursor->y;
		return;
	}

	struct wlr_surface *surface = NULL;
	double sx, sy;

	// Find the node beneath the pointer's previous position
	struct sway_node *prev_node = node_at_coords(seat,
			cursor->previous.x, cursor->previous.y, &surface, &sx, &sy);
	// Update the stored previous position
	cursor->previous.x = cursor->cursor->x;
	cursor->previous.y = cursor->cursor->y;

	struct sway_node *node = node_at_coords(seat,
			cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);
	if (node && config->focus_follows_mouse && allow_refocusing) {
		struct sway_node *focus = seat_get_focus(seat);
		if (focus && node->type == N_WORKSPACE) {
			// Only follow the mouse if it would move to a new output
			// Otherwise we'll focus the workspace, which is probably wrong
			struct sway_output *focused_output = node_get_output(focus);
			struct sway_output *output = node_get_output(node);
			if (output != focused_output) {
				seat_set_focus_warp(seat, node, false, true);
			}
		} else if (node->type == N_CONTAINER && node->sway_container->view) {
			// Focus node if the following are true:
			// - cursor is over a new view, i.e. entered a new window; and
			// - the new view is visible, i.e. not hidden in a stack or tab; and
			// - the seat does not have a keyboard grab
			if (!wlr_seat_keyboard_has_grab(cursor->seat->wlr_seat) &&
					node != prev_node &&
					view_is_visible(node->sway_container->view)) {
				seat_set_focus_warp(seat, node, false, true);
			} else {
				struct sway_node *next_focus =
					seat_get_focus_inactive(seat, &root->node);
				if (next_focus && next_focus->type == N_CONTAINER &&
						next_focus->sway_container->view &&
						view_is_visible(next_focus->sway_container->view)) {
					seat_set_focus_warp(seat, next_focus, false, true);
				}
			}
		}
	}

	// Handle cursor image
	if (surface) {
		// Reset cursor if switching between clients
		struct wl_client *client = wl_resource_get_client(surface->resource);
		if (client != cursor->image_client) {
			cursor_set_image(cursor, "left_ptr", client);
		}
	} else if (node && node->type == N_CONTAINER) {
		// Try a node's resize edge
		enum wlr_edges edge = find_resize_edge(node->sway_container, cursor);
		if (edge == WLR_EDGE_NONE) {
			cursor_set_image(cursor, "left_ptr", NULL);
		} else if (container_is_floating(node->sway_container)) {
			cursor_set_image(cursor, wlr_xcursor_get_resize_name(edge), NULL);
		} else {
			if (edge & (WLR_EDGE_LEFT | WLR_EDGE_RIGHT)) {
				cursor_set_image(cursor, "col-resize", NULL);
			} else {
				cursor_set_image(cursor, "row-resize", NULL);
			}
		}
	} else {
		cursor_set_image(cursor, "left_ptr", NULL);
	}

	// send pointer enter/leave
	if (surface != NULL) {
		if (seat_is_input_allowed(seat, surface)) {
			wlr_seat_pointer_notify_enter(wlr_seat, surface, sx, sy);
			wlr_seat_pointer_notify_motion(wlr_seat, time_msec, sx, sy);
		}
	} else {
		wlr_seat_pointer_clear_focus(wlr_seat);
	}

	struct wlr_drag_icon *wlr_drag_icon;
	wl_list_for_each(wlr_drag_icon, &wlr_seat->drag_icons, link) {
		struct sway_drag_icon *drag_icon = wlr_drag_icon->data;
		drag_icon_update_position(drag_icon);
	}
}

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, motion);
	wlr_idle_notify_activity(cursor->seat->input->server->idle, cursor->seat->wlr_seat);
	struct wlr_event_pointer_motion *event = data;
	wlr_cursor_move(cursor->cursor, event->device,
		event->delta_x, event->delta_y);
	cursor_send_pointer_motion(cursor, event->time_msec, true);
	transaction_commit_dirty();
}

static void handle_cursor_motion_absolute(
		struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, motion_absolute);
	wlr_idle_notify_activity(cursor->seat->input->server->idle, cursor->seat->wlr_seat);
	struct wlr_event_pointer_motion_absolute *event = data;
	wlr_cursor_warp_absolute(cursor->cursor, event->device, event->x, event->y);
	cursor_send_pointer_motion(cursor, event->time_msec, true);
	transaction_commit_dirty();
}

/**
 * Remove a button (and duplicates) to the sorted list of currently pressed buttons
 */
static void state_erase_button(struct sway_cursor *cursor, uint32_t button) {
	size_t j = 0;
	for (size_t i = 0; i < cursor->pressed_button_count; ++i) {
		if (i > j) {
			cursor->pressed_buttons[j] = cursor->pressed_buttons[i];
		}
		if (cursor->pressed_buttons[i] != button) {
			++j;
		}
	}
	while (cursor->pressed_button_count > j) {
		--cursor->pressed_button_count;
		cursor->pressed_buttons[cursor->pressed_button_count] = 0;
	}
}

/**
 * Add a button to the sorted list of currently pressed buttons, if there
 * is space.
 */
static void state_add_button(struct sway_cursor *cursor, uint32_t button) {
	if (cursor->pressed_button_count >= SWAY_CURSOR_PRESSED_BUTTONS_CAP) {
		return;
	}
	size_t i = 0;
	while (i < cursor->pressed_button_count && cursor->pressed_buttons[i] < button) {
		++i;
	}
	size_t j = cursor->pressed_button_count;
	while (j > i) {
		cursor->pressed_buttons[j] = cursor->pressed_buttons[j - 1];
		--j;
	}
	cursor->pressed_buttons[i] = button;
	cursor->pressed_button_count++;
}

/**
 * Return the mouse binding which matches modifier, click location, release,
 * and pressed button state, otherwise return null.
 */
static struct sway_binding* get_active_mouse_binding(const struct sway_cursor *cursor,
		list_t *bindings, uint32_t modifiers, bool release, bool on_titlebar,
				     bool on_border, bool on_content) {
	uint32_t click_region = (on_titlebar ? BINDING_TITLEBAR : 0) |
			(on_border ? BINDING_BORDER : 0) |
			(on_content ? BINDING_CONTENTS : 0);

	for (int i = 0; i < bindings->length; ++i) {
		struct sway_binding *binding = bindings->items[i];
		if (modifiers ^ binding->modifiers ||
				cursor->pressed_button_count != (size_t)binding->keys->length ||
				release != (binding->flags & BINDING_RELEASE) ||
				!(click_region & binding->flags)) {
			continue;
		}

		bool match = true;
		for (size_t j = 0; j < cursor->pressed_button_count; j++) {
			uint32_t key = *(uint32_t *)binding->keys->items[j];
			if (key != cursor->pressed_buttons[j]) {
				match = false;
				break;
			}
		}
		if (!match) {
			continue;
		}

		return binding;
	}
	return NULL;
}

void dispatch_cursor_button(struct sway_cursor *cursor,
		uint32_t time_msec, uint32_t button, enum wlr_button_state state) {
	if (time_msec == 0) {
		time_msec = get_current_time_msec();
	}
	struct sway_seat *seat = cursor->seat;

	// Handle ending seat operation
	if (cursor->seat->operation != OP_NONE &&
			button == cursor->seat->op_button && state == WLR_BUTTON_RELEASED) {
		seat_end_mouse_operation(seat);
		seat_pointer_notify_button(seat, time_msec, button, state);
		return;
	}

	// Determine what's under the cursor
	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct sway_node *node = node_at_coords(seat,
			cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);
	struct sway_container *cont = node && node->type == N_CONTAINER ?
		node->sway_container : NULL;
	bool is_floating = cont && container_is_floating(cont);
	bool is_floating_or_child = cont && container_is_floating_or_child(cont);
	bool is_fullscreen_or_child = cont && container_is_fullscreen_or_child(cont);
	enum wlr_edges edge = cont ? find_edge(cont, cursor) : WLR_EDGE_NONE;
	enum wlr_edges resize_edge = edge ?
		find_resize_edge(cont, cursor) : WLR_EDGE_NONE;
	bool on_border = edge != WLR_EDGE_NONE;
	bool on_contents = cont && !on_border && surface;
	bool on_titlebar = cont && !on_border && !surface;

	// Handle mouse bindings
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat->wlr_seat);
	uint32_t modifiers = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;

	struct sway_binding *binding = NULL;
	if (state == WLR_BUTTON_PRESSED) {
		state_add_button(cursor, button);
		binding = get_active_mouse_binding(cursor,
			config->current_mode->mouse_bindings, modifiers, false,
			on_titlebar, on_border, on_contents);
	} else {
		binding = get_active_mouse_binding(cursor,
			config->current_mode->mouse_bindings, modifiers, true,
			on_titlebar, on_border, on_contents);
		state_erase_button(cursor, button);
	}
	if (binding) {
		seat_execute_command(seat, binding);
		return;
	}

	// Handle clicking an empty workspace
	if (node && node->type == N_WORKSPACE) {
		seat_set_focus(seat, node);
		return;
	}

	// Handle clicking a layer surface
	if (surface && wlr_surface_is_layer_surface(surface)) {
		struct wlr_layer_surface *layer =
			wlr_layer_surface_from_wlr_surface(surface);
		if (layer->current.keyboard_interactive) {
			seat_set_focus_layer(seat, layer);
		}
		seat_pointer_notify_button(seat, time_msec, button, state);
		return;
	}

	// Handle tiling resize via border
	if (resize_edge && button == BTN_LEFT && state == WLR_BUTTON_PRESSED &&
			!is_floating) {
		seat_set_focus_container(seat, cont);
		seat_begin_resize_tiling(seat, cont, button, edge);
		return;
	}

	// Handle tiling resize via mod
	bool mod_pressed = keyboard &&
		(wlr_keyboard_get_modifiers(keyboard) & config->floating_mod);
	if (!is_floating_or_child && mod_pressed && state == WLR_BUTTON_PRESSED) {
		uint32_t btn_resize = config->floating_mod_inverse ?
			BTN_LEFT : BTN_RIGHT;
		if (button == btn_resize) {
			edge = 0;
			edge |= cursor->cursor->x > cont->x + cont->width / 2 ?
				WLR_EDGE_RIGHT : WLR_EDGE_LEFT;
			edge |= cursor->cursor->y > cont->y + cont->height / 2 ?
				WLR_EDGE_BOTTOM : WLR_EDGE_TOP;

			const char *image = NULL;
			if (edge == (WLR_EDGE_LEFT | WLR_EDGE_TOP)) {
				image = "nw-resize";
			} else if (edge == (WLR_EDGE_TOP | WLR_EDGE_RIGHT)) {
				image = "ne-resize";
			} else if (edge == (WLR_EDGE_RIGHT | WLR_EDGE_BOTTOM)) {
				image = "se-resize";
			} else if (edge == (WLR_EDGE_BOTTOM | WLR_EDGE_LEFT)) {
				image = "sw-resize";
			}
			cursor_set_image(seat->cursor, image, NULL);
			seat_set_focus_container(seat, cont);
			seat_begin_resize_tiling(seat, cont, button, edge);
			return;
		}
	}

	// Handle beginning floating move
	if (is_floating_or_child && !is_fullscreen_or_child &&
			state == WLR_BUTTON_PRESSED) {
		uint32_t btn_move = config->floating_mod_inverse ? BTN_RIGHT : BTN_LEFT;
		if (button == btn_move && state == WLR_BUTTON_PRESSED &&
				(mod_pressed || on_titlebar)) {
			while (cont->parent) {
				cont = cont->parent;
			}
			seat_begin_move_floating(seat, cont, button);
			return;
		}
	}

	// Handle beginning floating resize
	if (is_floating_or_child && !is_fullscreen_or_child &&
			state == WLR_BUTTON_PRESSED) {
		// Via border
		if (button == BTN_LEFT && resize_edge != WLR_EDGE_NONE) {
			seat_begin_resize_floating(seat, cont, button, resize_edge);
			return;
		}

		// Via mod+click
		uint32_t btn_resize = config->floating_mod_inverse ?
			BTN_LEFT : BTN_RIGHT;
		if (mod_pressed && button == btn_resize) {
			struct sway_container *floater = cont;
			while (floater->parent) {
				floater = floater->parent;
			}
			edge = 0;
			edge |= cursor->cursor->x > floater->x + floater->width / 2 ?
				WLR_EDGE_RIGHT : WLR_EDGE_LEFT;
			edge |= cursor->cursor->y > floater->y + floater->height / 2 ?
				WLR_EDGE_BOTTOM : WLR_EDGE_TOP;
			seat_begin_resize_floating(seat, floater, button, edge);
			return;
		}
	}

	// Handle moving a tiling container
	if (config->tiling_drag && mod_pressed && state == WLR_BUTTON_PRESSED &&
			!is_floating_or_child && !cont->is_fullscreen) {
		seat_pointer_notify_button(seat, time_msec, button, state);
		seat_begin_move_tiling(seat, cont, button);
		return;
	}

	// Handle mousedown on a container surface
	if (surface && cont && state == WLR_BUTTON_PRESSED) {
		seat_set_focus_container(seat, cont);
		seat_pointer_notify_button(seat, time_msec, button, state);
		seat_begin_down(seat, cont, button, sx, sy);
		return;
	}

	// Handle clicking a container surface
	if (cont) {
		seat_set_focus_container(seat, cont);
		seat_pointer_notify_button(seat, time_msec, button, state);
		return;
	}

	seat_pointer_notify_button(seat, time_msec, button, state);
}

static void handle_cursor_button(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, button);
	wlr_idle_notify_activity(cursor->seat->input->server->idle, cursor->seat->wlr_seat);
	struct wlr_event_pointer_button *event = data;
	dispatch_cursor_button(cursor,
			event->time_msec, event->button, event->state);
	transaction_commit_dirty();
}

static void handle_cursor_axis(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, axis);
	wlr_idle_notify_activity(cursor->seat->input->server->idle, cursor->seat->wlr_seat);
	struct wlr_event_pointer_axis *event = data;
	wlr_seat_pointer_notify_axis(cursor->seat->wlr_seat, event->time_msec,
		event->orientation, event->delta, event->delta_discrete, event->source);
}

static void handle_touch_down(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, touch_down);
	wlr_idle_notify_activity(cursor->seat->input->server->idle, cursor->seat->wlr_seat);
	struct wlr_event_touch_down *event = data;

	struct sway_seat *seat = cursor->seat;
	struct wlr_seat *wlr_seat = seat->wlr_seat;
	struct wlr_surface *surface = NULL;

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(cursor->cursor, event->device,
			event->x, event->y, &lx, &ly);
	double sx, sy;
	node_at_coords(seat, lx, ly, &surface, &sx, &sy);

	seat->touch_id = event->touch_id;
	seat->touch_x = lx;
	seat->touch_y = ly;

	if (!surface) {
		return;
	}

	// TODO: fall back to cursor simulation if client has not bound to touch
	if (seat_is_input_allowed(seat, surface)) {
		wlr_seat_touch_notify_down(wlr_seat, surface, event->time_msec,
				event->touch_id, sx, sy);
		cursor->image_client = NULL;
		wlr_cursor_set_image(cursor->cursor, NULL, 0, 0, 0, 0, 0, 0);
	}
}

static void handle_touch_up(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, touch_up);
	wlr_idle_notify_activity(cursor->seat->input->server->idle, cursor->seat->wlr_seat);
	struct wlr_event_touch_up *event = data;
	struct wlr_seat *seat = cursor->seat->wlr_seat;
	// TODO: fall back to cursor simulation if client has not bound to touch
	wlr_seat_touch_notify_up(seat, event->time_msec, event->touch_id);
}

static void handle_touch_motion(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, touch_motion);
	wlr_idle_notify_activity(cursor->seat->input->server->idle, cursor->seat->wlr_seat);
	struct wlr_event_touch_motion *event = data;

	struct sway_seat *seat = cursor->seat;
	struct wlr_seat *wlr_seat = seat->wlr_seat;
	struct wlr_surface *surface = NULL;

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(cursor->cursor, event->device,
			event->x, event->y, &lx, &ly);
	double sx, sy;
	node_at_coords(cursor->seat, lx, ly, &surface, &sx, &sy);

	if (seat->touch_id == event->touch_id) {
		seat->touch_x = lx;
		seat->touch_y = ly;

		struct wlr_drag_icon *wlr_drag_icon;
		wl_list_for_each(wlr_drag_icon, &wlr_seat->drag_icons, link) {
			struct sway_drag_icon *drag_icon = wlr_drag_icon->data;
			drag_icon_update_position(drag_icon);
		}
	}

	if (!surface) {
		return;
	}

	// TODO: fall back to cursor simulation if client has not bound to touch
	if (seat_is_input_allowed(cursor->seat, surface)) {
		wlr_seat_touch_notify_motion(wlr_seat, event->time_msec,
			event->touch_id, sx, sy);
	}
}

static double apply_mapping_from_coord(double low, double high, double value) {
	if (isnan(value)) {
		return value;
	}

	return (value - low) / (high - low);
}

static void apply_mapping_from_region(struct wlr_input_device *device,
		struct input_config_mapped_from_region *region, double *x, double *y) {
	double x1 = region->x1, x2 = region->x2;
	double y1 = region->y1, y2 = region->y2;

	if (region->mm) {
		if (device->width_mm == 0 || device->height_mm == 0) {
			return;
		}
		x1 /= device->width_mm;
		x2 /= device->width_mm;
		y1 /= device->height_mm;
		y2 /= device->height_mm;
	}

	*x = apply_mapping_from_coord(x1, x2, *x);
	*y = apply_mapping_from_coord(y1, y2, *y);
}

static void handle_tool_axis(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, tool_axis);
	wlr_idle_notify_activity(cursor->seat->input->server->idle, cursor->seat->wlr_seat);
	struct wlr_event_tablet_tool_axis *event = data;
	struct sway_input_device *input_device = event->device->data;

	double x = NAN, y = NAN;
	if ((event->updated_axes & WLR_TABLET_TOOL_AXIS_X)) {
		x = event->x;
	}
	if ((event->updated_axes & WLR_TABLET_TOOL_AXIS_Y)) {
		y = event->y;
	}

	struct input_config *ic = input_device_get_config(input_device);
	if (ic != NULL && ic->mapped_from_region != NULL) {
		apply_mapping_from_region(event->device, ic->mapped_from_region, &x, &y);
	}

	wlr_cursor_warp_absolute(cursor->cursor, event->device, x, y);
	cursor_send_pointer_motion(cursor, event->time_msec, true);
	transaction_commit_dirty();
}

static void handle_tool_tip(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, tool_tip);
	wlr_idle_notify_activity(cursor->seat->input->server->idle, cursor->seat->wlr_seat);
	struct wlr_event_tablet_tool_tip *event = data;
	dispatch_cursor_button(cursor, event->time_msec,
			BTN_LEFT, event->state == WLR_TABLET_TOOL_TIP_DOWN ?
				WLR_BUTTON_PRESSED : WLR_BUTTON_RELEASED);
	transaction_commit_dirty();
}

static void handle_tool_button(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, tool_button);
	wlr_idle_notify_activity(cursor->seat->input->server->idle, cursor->seat->wlr_seat);
	struct wlr_event_tablet_tool_button *event = data;
	// TODO: the user may want to configure which tool buttons are mapped to
	// which simulated pointer buttons
	switch (event->state) {
	case WLR_BUTTON_PRESSED:
		if (cursor->tool_buttons == 0) {
			dispatch_cursor_button(cursor,
					event->time_msec, BTN_RIGHT, event->state);
		}
		cursor->tool_buttons++;
		break;
	case WLR_BUTTON_RELEASED:
		if (cursor->tool_buttons == 1) {
			dispatch_cursor_button(cursor,
					event->time_msec, BTN_RIGHT, event->state);
		}
		cursor->tool_buttons--;
		break;
	}
	transaction_commit_dirty();
}

static void handle_request_set_cursor(struct wl_listener *listener,
		void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, request_set_cursor);
	if (cursor->seat->operation != OP_NONE) {
		return;
	}
	struct wlr_seat_pointer_request_set_cursor_event *event = data;

	struct wl_client *focused_client = NULL;
	struct wlr_surface *focused_surface =
		cursor->seat->wlr_seat->pointer_state.focused_surface;
	if (focused_surface != NULL) {
		focused_client = wl_resource_get_client(focused_surface->resource);
	}

	// TODO: check cursor mode
	if (focused_client == NULL ||
			event->seat_client->client != focused_client) {
		wlr_log(WLR_DEBUG, "denying request to set cursor from unfocused client");
		return;
	}

	wlr_cursor_set_surface(cursor->cursor, event->surface, event->hotspot_x,
		event->hotspot_y);
	cursor->image = NULL;
	cursor->image_client = focused_client;
}

void cursor_set_image(struct sway_cursor *cursor, const char *image,
		struct wl_client *client) {
	if (!cursor->image || strcmp(cursor->image, image) != 0) {
		wlr_xcursor_manager_set_cursor_image(cursor->xcursor_manager, image,
				cursor->cursor);
		cursor->image = image;
	}
	cursor->image_client = client;
}

void sway_cursor_destroy(struct sway_cursor *cursor) {
	if (!cursor) {
		return;
	}

	wlr_xcursor_manager_destroy(cursor->xcursor_manager);
	wlr_cursor_destroy(cursor->cursor);
	free(cursor);
}

struct sway_cursor *sway_cursor_create(struct sway_seat *seat) {
	struct sway_cursor *cursor = calloc(1, sizeof(struct sway_cursor));
	if (!sway_assert(cursor, "could not allocate sway cursor")) {
		return NULL;
	}

	struct wlr_cursor *wlr_cursor = wlr_cursor_create();
	if (!sway_assert(wlr_cursor, "could not allocate wlr cursor")) {
		free(cursor);
		return NULL;
	}

	cursor->previous.x = wlr_cursor->x;
	cursor->previous.y = wlr_cursor->y;

	cursor->seat = seat;
	wlr_cursor_attach_output_layout(wlr_cursor, root->output_layout);

	// input events
	wl_signal_add(&wlr_cursor->events.motion, &cursor->motion);
	cursor->motion.notify = handle_cursor_motion;

	wl_signal_add(&wlr_cursor->events.motion_absolute,
		&cursor->motion_absolute);
	cursor->motion_absolute.notify = handle_cursor_motion_absolute;

	wl_signal_add(&wlr_cursor->events.button, &cursor->button);
	cursor->button.notify = handle_cursor_button;

	wl_signal_add(&wlr_cursor->events.axis, &cursor->axis);
	cursor->axis.notify = handle_cursor_axis;

	wl_signal_add(&wlr_cursor->events.touch_down, &cursor->touch_down);
	cursor->touch_down.notify = handle_touch_down;

	wl_signal_add(&wlr_cursor->events.touch_up, &cursor->touch_up);
	cursor->touch_up.notify = handle_touch_up;

	wl_signal_add(&wlr_cursor->events.touch_motion,
		&cursor->touch_motion);
	cursor->touch_motion.notify = handle_touch_motion;

	// TODO: tablet protocol support
	// Note: We should emulate pointer events for clients that don't support the
	// tablet protocol when the time comes
	wl_signal_add(&wlr_cursor->events.tablet_tool_axis,
		&cursor->tool_axis);
	cursor->tool_axis.notify = handle_tool_axis;

	wl_signal_add(&wlr_cursor->events.tablet_tool_tip, &cursor->tool_tip);
	cursor->tool_tip.notify = handle_tool_tip;

	wl_signal_add(&wlr_cursor->events.tablet_tool_button, &cursor->tool_button);
	cursor->tool_button.notify = handle_tool_button;

	wl_signal_add(&seat->wlr_seat->events.request_set_cursor,
			&cursor->request_set_cursor);
	cursor->request_set_cursor.notify = handle_request_set_cursor;

	cursor->cursor = wlr_cursor;

	return cursor;
}
