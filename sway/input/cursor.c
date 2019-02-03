#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <math.h>
#include <libevdev/libevdev.h>
#include <linux/input-event-codes.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <strings.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_idle.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/region.h>
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

static uint32_t get_current_time_msec(void) {
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
struct sway_node *node_at_coords(
		struct sway_seat *seat, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	// check for unmanaged views first
#if HAVE_XWAYLAND
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
	if (!output || !output->configured) {
		// output is being destroyed or is being configured
		return NULL;
	}
	double ox = lx, oy = ly;
	wlr_output_layout_output_coords(root->output_layout, wlr_output, &ox, &oy);

	if (root->fullscreen_global) {
		// Try fullscreen container
		struct sway_container *con = tiling_container_at(
				&root->fullscreen_global->node, lx, ly, surface, sx, sy);
		if (con) {
			return &con->node;
		}
		return NULL;
	}

	// find the focused workspace on the output for this seat
	struct sway_workspace *ws = output_get_active_workspace(output);

	if ((*surface = layer_surface_at(output,
				&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],
				ox, oy, sx, sy))) {
		return NULL;
	}
	if (ws->fullscreen) {
		// Try transient containers
		for (int i = 0; i < ws->floating->length; ++i) {
			struct sway_container *floater = ws->floating->items[i];
			if (container_is_transient_for(floater, ws->fullscreen)) {
				struct sway_container *con = tiling_container_at(
						&floater->node, lx, ly, surface, sx, sy);
				if (con) {
					return &con->node;
				}
			}
		}
		// Try fullscreen container
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
	if (cont->border == B_NONE || !cont->border_thickness ||
			cont->border == B_CSD) {
		return WLR_EDGE_NONE;
	}

	enum wlr_edges edge = 0;
	if (cursor->cursor->x < cont->x + cont->border_thickness) {
		edge |= WLR_EDGE_LEFT;
	}
	if (cursor->cursor->y < cont->y + cont->border_thickness) {
		edge |= WLR_EDGE_TOP;
	}
	if (cursor->cursor->x >= cont->x + cont->width - cont->border_thickness) {
		edge |= WLR_EDGE_RIGHT;
	}
	if (cursor->cursor->y >= cont->y + cont->height - cont->border_thickness) {
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

static void cursor_do_rebase(struct sway_cursor *cursor, uint32_t time_msec,
		struct sway_node *node, struct wlr_surface *surface,
		double sx, double sy) {
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

	// Send pointer enter/leave
	struct wlr_seat *wlr_seat = cursor->seat->wlr_seat;
	if (surface) {
		if (seat_is_input_allowed(cursor->seat, surface)) {
			wlr_seat_pointer_notify_enter(wlr_seat, surface, sx, sy);
			wlr_seat_pointer_notify_motion(wlr_seat, time_msec, sx, sy);
		}
	} else {
		wlr_seat_pointer_clear_focus(wlr_seat);
	}
}

void cursor_rebase(struct sway_cursor *cursor) {
	uint32_t time_msec = get_current_time_msec();
	struct wlr_surface *surface = NULL;
	double sx, sy;
	cursor->previous.node = node_at_coords(cursor->seat,
			cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);
	cursor_do_rebase(cursor, time_msec, cursor->previous.node, surface, sx, sy);
}

static int hide_notify(void *data) {
	struct sway_cursor *cursor = data;
	wlr_cursor_set_image(cursor->cursor, NULL, 0, 0, 0, 0, 0, 0);
	cursor->hidden = true;
	wlr_seat_pointer_clear_focus(cursor->seat->wlr_seat);
	return 1;
}

int cursor_get_timeout(struct sway_cursor *cursor){
	struct seat_config *sc = seat_get_config(cursor->seat);
	if (!sc) {
		sc = seat_get_config_by_name("*");
	}
	int timeout = sc ? sc->hide_cursor_timeout : 0;
	if (timeout < 0) {
		timeout = 0;
	}
	return timeout;
}

void cursor_handle_activity(struct sway_cursor *cursor) {
	wl_event_source_timer_update(
			cursor->hide_source, cursor_get_timeout(cursor));

	wlr_idle_notify_activity(server.idle, cursor->seat->wlr_seat);
	if (cursor->hidden) {
		cursor_unhide(cursor);
	}
}

void cursor_unhide(struct sway_cursor *cursor) {
	cursor->hidden = false;
	if (cursor->image_surface) {
		cursor_set_image_surface(cursor,
				cursor->image_surface,
				cursor->hotspot_x,
				cursor->hotspot_y,
				cursor->image_client);
	} else {
		const char *image = cursor->image;
		cursor->image = NULL;
		cursor_set_image(cursor, image, cursor->image_client);
	}
	cursor_rebase(cursor);
}

void cursor_send_pointer_motion(struct sway_cursor *cursor, uint32_t time_msec,
		struct sway_node *node, struct wlr_surface *surface,
		double sx, double sy) {
	if (time_msec == 0) {
		time_msec = get_current_time_msec();
	}

	struct sway_seat *seat = cursor->seat;
	struct wlr_seat *wlr_seat = seat->wlr_seat;

	if (seat_doing_seatop(seat)) {
		seatop_motion(seat, time_msec);
		cursor->previous.x = cursor->cursor->x;
		cursor->previous.y = cursor->cursor->y;
		return;
	}

	struct sway_node *prev_node = cursor->previous.node;

	// Update the stored previous position
	cursor->previous.x = cursor->cursor->x;
	cursor->previous.y = cursor->cursor->y;
	cursor->previous.node = node;

	if (node && (config->focus_follows_mouse == FOLLOWS_YES ||
			config->focus_follows_mouse == FOLLOWS_ALWAYS)) {
		struct sway_node *focus = seat_get_focus(seat);
		if (focus && node->type == N_WORKSPACE) {
			// Only follow the mouse if it would move to a new output
			// Otherwise we'll focus the workspace, which is probably wrong
			struct sway_output *focused_output = node_get_output(focus);
			struct sway_output *output = node_get_output(node);
			if (output != focused_output) {
				seat_set_focus(seat, seat_get_focus_inactive(seat, node));
			}
		} else if (node->type == N_CONTAINER && node->sway_container->view) {
			// Focus node if the following are true:
			// - cursor is over a new view, i.e. entered a new window; and
			// - the new view is visible, i.e. not hidden in a stack or tab; and
			// - the seat does not have a keyboard grab
			if ((!wlr_seat_keyboard_has_grab(cursor->seat->wlr_seat) &&
					node != prev_node &&
					view_is_visible(node->sway_container->view)) ||
					config->focus_follows_mouse == FOLLOWS_ALWAYS) {
				seat_set_focus(seat, node);
			} else {
				struct sway_node *next_focus =
					seat_get_focus_inactive(seat, &root->node);
				if (next_focus && next_focus->type == N_CONTAINER &&
						next_focus->sway_container->view &&
						view_is_visible(next_focus->sway_container->view)) {
					seat_set_focus(seat, next_focus);
				}
			}
		}
	}

	cursor_do_rebase(cursor, time_msec, node, surface, sx, sy);

	struct wlr_drag_icon *wlr_drag_icon;
	wl_list_for_each(wlr_drag_icon, &wlr_seat->drag_icons, link) {
		struct sway_drag_icon *drag_icon = wlr_drag_icon->data;
		drag_icon_update_position(drag_icon);
	}
}

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, motion);
	struct wlr_event_pointer_motion *event = data;
	cursor_handle_activity(cursor);

	double dx = event->delta_x;
	double dy = event->delta_y;

	double dx_unaccel = event->unaccel_dx;
	double dy_unaccel = event->unaccel_dy;

	wlr_relative_pointer_manager_v1_send_relative_motion(
		server.relative_pointer_manager,
		cursor->seat->wlr_seat, event->time_msec, dx, dy,
		dx_unaccel, dy_unaccel);

	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct sway_node *node = node_at_coords(cursor->seat,
		cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);

	if (cursor->active_constraint) {
		if (cursor->active_constraint->surface != surface) {
			return;
		}

		double sx_confined, sy_confined;
		if (!wlr_region_confine(&cursor->confine, sx, sy, sx + dx, sy + dy,
				&sx_confined, &sy_confined)) {
			return;
		}

		dx = sx_confined - sx;
		dy = sy_confined - sy;
	}

	wlr_cursor_move(cursor->cursor, event->device, dx, dy);
	cursor_send_pointer_motion(cursor, event->time_msec, node, surface,
		sx + dx, sy + dy);
	transaction_commit_dirty();
}

static void cursor_motion_absolute(struct sway_cursor *cursor,
		uint32_t time_msec, struct wlr_input_device *dev,
		double x, double y) {
	cursor_handle_activity(cursor);

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(cursor->cursor, dev,
		x, y, &lx, &ly);

	double dx = lx - cursor->cursor->x;
	double dy = ly - cursor->cursor->y;
	wlr_relative_pointer_manager_v1_send_relative_motion(
		server.relative_pointer_manager,
		cursor->seat->wlr_seat, (uint64_t)time_msec * 1000,
		dx, dy, dx, dy);

	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct sway_node *node = node_at_coords(cursor->seat,
		lx, ly, &surface, &sx, &sy);

	if (cursor->active_constraint) {
		if (cursor->active_constraint->surface != surface) {
			return;
		}
		if (!pixman_region32_contains_point(&cursor->confine,
				floor(sx), floor(sy), NULL)) {
			return;
		}
	}

	wlr_cursor_warp_closest(cursor->cursor, dev, lx, ly);
	cursor_send_pointer_motion(cursor, time_msec, node, surface, sx, sy);
	transaction_commit_dirty();
}

static void handle_cursor_motion_absolute(
		struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, motion_absolute);
	struct wlr_event_pointer_motion_absolute *event = data;

	cursor_motion_absolute(cursor, event->time_msec, event->device,
		event->x, event->y);
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
static struct sway_binding* get_active_mouse_binding(
		const struct sway_cursor *cursor, list_t *bindings, uint32_t modifiers,
		bool release, bool on_titlebar, bool on_border, bool on_content,
		const char *identifier) {
	uint32_t click_region = (on_titlebar ? BINDING_TITLEBAR : 0) |
			(on_border ? BINDING_BORDER : 0) |
			(on_content ? BINDING_CONTENTS : 0);

	struct sway_binding *current = NULL;
	for (int i = 0; i < bindings->length; ++i) {
		struct sway_binding *binding = bindings->items[i];
		if (modifiers ^ binding->modifiers ||
				cursor->pressed_button_count != (size_t)binding->keys->length ||
				release != (binding->flags & BINDING_RELEASE) ||
				!(click_region & binding->flags) ||
				(strcmp(binding->input, identifier) != 0 &&
				 strcmp(binding->input, "*") != 0)) {
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

		if (!current || strcmp(current->input, "*") == 0) {
			current = binding;
			if (strcmp(current->input, identifier) == 0) {
				// If a binding is found for the exact input, quit searching
				break;
			}
		}
	}
	return current;
}

void dispatch_cursor_button(struct sway_cursor *cursor,
		struct wlr_input_device *device, uint32_t time_msec, uint32_t button,
		enum wlr_button_state state) {
	if (time_msec == 0) {
		time_msec = get_current_time_msec();
	}
	struct sway_seat *seat = cursor->seat;

	// Handle existing seat operation
	if (seat_doing_seatop(seat)) {
		if (button == seat->seatop_button && state == WLR_BUTTON_RELEASED) {
			seatop_finish(seat);
			seat_pointer_notify_button(seat, time_msec, button, state);
		}
		if (state == WLR_BUTTON_PRESSED) {
			state_add_button(cursor, button);
		} else {
			state_erase_button(cursor, button);
		}
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

	char *device_identifier = device ? input_device_get_identifier(device)
		: strdup("*");
	struct sway_binding *binding = NULL;
	if (state == WLR_BUTTON_PRESSED) {
		state_add_button(cursor, button);
		binding = get_active_mouse_binding(cursor,
			config->current_mode->mouse_bindings, modifiers, false,
			on_titlebar, on_border, on_contents, device_identifier);
	} else {
		binding = get_active_mouse_binding(cursor,
			config->current_mode->mouse_bindings, modifiers, true,
			on_titlebar, on_border, on_contents, device_identifier);
		state_erase_button(cursor, button);
	}
	free(device_identifier);
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
		struct wlr_layer_surface_v1 *layer =
			wlr_layer_surface_v1_from_wlr_surface(surface);
		if (layer->current.keyboard_interactive) {
			seat_set_focus_layer(seat, layer);
		}
		seat_pointer_notify_button(seat, time_msec, button, state);
		return;
	}

	// Handle tiling resize via border
	if (cont && resize_edge && button == BTN_LEFT &&
			state == WLR_BUTTON_PRESSED && !is_floating) {
		seat_set_focus_container(seat, cont);
		seatop_begin_resize_tiling(seat, cont, button, edge);
		return;
	}

	// Handle tiling resize via mod
	bool mod_pressed = keyboard &&
		(wlr_keyboard_get_modifiers(keyboard) & config->floating_mod);
	if (cont && !is_floating_or_child && mod_pressed &&
			state == WLR_BUTTON_PRESSED) {
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
			seatop_begin_resize_tiling(seat, cont, button, edge);
			return;
		}
	}

	// Handle beginning floating move
	if (cont && is_floating_or_child && !is_fullscreen_or_child &&
			state == WLR_BUTTON_PRESSED) {
		uint32_t btn_move = config->floating_mod_inverse ? BTN_RIGHT : BTN_LEFT;
		if (button == btn_move && state == WLR_BUTTON_PRESSED &&
				(mod_pressed || on_titlebar)) {
			while (cont->parent) {
				cont = cont->parent;
			}
			seat_set_focus_container(seat, cont);
			seatop_begin_move_floating(seat, cont, button);
			return;
		}
	}

	// Handle beginning floating resize
	if (cont && is_floating_or_child && !is_fullscreen_or_child &&
			state == WLR_BUTTON_PRESSED) {
		// Via border
		if (button == BTN_LEFT && resize_edge != WLR_EDGE_NONE) {
			seatop_begin_resize_floating(seat, cont, button, resize_edge);
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
			seatop_begin_resize_floating(seat, floater, button, edge);
			return;
		}
	}

	// Handle moving a tiling container
	if (config->tiling_drag && (mod_pressed || on_titlebar) &&
			state == WLR_BUTTON_PRESSED && !is_floating_or_child &&
			cont && cont->fullscreen_mode == FULLSCREEN_NONE) {
		struct sway_container *focus = seat_get_focused_container(seat);
		bool focused = focus == cont || container_has_ancestor(focus, cont);
		if (on_titlebar && !focused) {
			node = seat_get_focus_inactive(seat, &cont->node);
			seat_set_focus(seat, node);
		}

		seat_pointer_notify_button(seat, time_msec, button, state);

		// If moving a container by it's title bar, use a threshold for the drag
		if (!mod_pressed && config->tiling_drag_threshold > 0) {
			seatop_begin_move_tiling_threshold(seat, cont, button);
		} else {
			seatop_begin_move_tiling(seat, cont, button);
		}
		return;
	}

	// Handle mousedown on a container surface
	if (surface && cont && state == WLR_BUTTON_PRESSED) {
		seat_set_focus_container(seat, cont);
		seat_pointer_notify_button(seat, time_msec, button, state);
		seatop_begin_down(seat, cont, button, sx, sy);
		return;
	}

	// Handle clicking a container surface or decorations
	if (cont) {
		node = seat_get_focus_inactive(seat, &cont->node);
		seat_set_focus(seat, node);
		seat_pointer_notify_button(seat, time_msec, button, state);
		return;
	}

	seat_pointer_notify_button(seat, time_msec, button, state);
}

static void handle_cursor_button(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, button);
	struct wlr_event_pointer_button *event = data;
	cursor_handle_activity(cursor);
	dispatch_cursor_button(cursor, event->device,
			event->time_msec, event->button, event->state);
	transaction_commit_dirty();
}

static uint32_t wl_axis_to_button(struct wlr_event_pointer_axis *event) {
	switch (event->orientation) {
	case WLR_AXIS_ORIENTATION_VERTICAL:
		return event->delta < 0 ? SWAY_SCROLL_UP : SWAY_SCROLL_DOWN;
	case WLR_AXIS_ORIENTATION_HORIZONTAL:
		return event->delta < 0 ? SWAY_SCROLL_LEFT : SWAY_SCROLL_RIGHT;
	default:
		sway_log(SWAY_DEBUG, "Unknown axis orientation");
		return 0;
	}
}

void dispatch_cursor_axis(struct sway_cursor *cursor,
		struct wlr_event_pointer_axis *event) {
	struct sway_seat *seat = cursor->seat;
	struct sway_input_device *input_device =
		event->device ? event->device->data : NULL;
	struct input_config *ic =
		input_device ? input_device_get_config(input_device) : NULL;

	// Determine what's under the cursor
	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct sway_node *node = node_at_coords(seat,
			cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);
	struct sway_container *cont = node && node->type == N_CONTAINER ?
		node->sway_container : NULL;
	enum wlr_edges edge = cont ? find_edge(cont, cursor) : WLR_EDGE_NONE;
	bool on_border = edge != WLR_EDGE_NONE;
	bool on_titlebar = cont && !on_border && !surface;
	bool on_titlebar_border = cont && on_border &&
		cursor->cursor->y < cont->content_y;
	bool on_contents = cont && !on_border && surface;
	float scroll_factor =
		(ic == NULL || ic->scroll_factor == FLT_MIN) ? 1.0f : ic->scroll_factor;

	bool handled = false;

	// Gather information needed for mouse bindings
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat->wlr_seat);
	uint32_t modifiers = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;
	struct wlr_input_device *device =
		input_device ? input_device->wlr_device : NULL;
	char *dev_id = device ? input_device_get_identifier(device) : strdup("*");
	uint32_t button = wl_axis_to_button(event);

	// Handle mouse bindings - x11 mouse buttons 4-7 - press event
	struct sway_binding *binding = NULL;
	state_add_button(cursor, button);
	binding = get_active_mouse_binding(cursor,
		config->current_mode->mouse_bindings, modifiers, false,
		on_titlebar, on_border, on_contents, dev_id);
	if (binding) {
		seat_execute_command(seat, binding);
		handled = true;
	}

	// Scrolling on a tabbed or stacked title bar (handled as press event)
	if (!handled && (on_titlebar || on_titlebar_border)) {
		enum sway_container_layout layout = container_parent_layout(cont);
		if (layout == L_TABBED || layout == L_STACKED) {
			struct sway_node *tabcontainer = node_get_parent(node);
			struct sway_node *active =
				seat_get_active_tiling_child(seat, tabcontainer);
			list_t *siblings = container_get_siblings(cont);
			int desired = list_find(siblings, active->sway_container) +
				round(scroll_factor * event->delta_discrete);
			if (desired < 0) {
				desired = 0;
			} else if (desired >= siblings->length) {
				desired = siblings->length - 1;
			}
			struct sway_node *old_focus = seat_get_focus(seat);
			struct sway_container *new_sibling_con = siblings->items[desired];
			struct sway_node *new_sibling = &new_sibling_con->node;
			struct sway_node *new_focus =
				seat_get_focus_inactive(seat, new_sibling);
			if (node_has_ancestor(old_focus, tabcontainer)) {
				seat_set_focus(seat, new_focus);
			} else {
				// Scrolling when focus is not in the tabbed container at all
				seat_set_raw_focus(seat, new_sibling);
				seat_set_raw_focus(seat, new_focus);
				seat_set_raw_focus(seat, old_focus);
			}
			handled = true;
		}
	}

	// Handle mouse bindings - x11 mouse buttons 4-7 - release event
	binding = get_active_mouse_binding(cursor,
		config->current_mode->mouse_bindings, modifiers, true,
		on_titlebar, on_border, on_contents, dev_id);
	state_erase_button(cursor, button);
	if (binding) {
		seat_execute_command(seat, binding);
		handled = true;
	}
	free(dev_id);

	if (!handled) {
		wlr_seat_pointer_notify_axis(cursor->seat->wlr_seat, event->time_msec,
			event->orientation, scroll_factor * event->delta,
			round(scroll_factor * event->delta_discrete), event->source);
	}
}

static void handle_cursor_axis(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, axis);
	struct wlr_event_pointer_axis *event = data;
	cursor_handle_activity(cursor);
	dispatch_cursor_axis(cursor, event);
	transaction_commit_dirty();
}

static void handle_cursor_frame(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, frame);
	cursor_handle_activity(cursor);
	wlr_seat_pointer_notify_frame(cursor->seat->wlr_seat);
}

static void handle_touch_down(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, touch_down);
	wlr_idle_notify_activity(server.idle, cursor->seat->wlr_seat);
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
		cursor_set_image(cursor, NULL, NULL);
	}
}

static void handle_touch_up(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, touch_up);
	wlr_idle_notify_activity(server.idle, cursor->seat->wlr_seat);
	struct wlr_event_touch_up *event = data;
	struct wlr_seat *seat = cursor->seat->wlr_seat;
	// TODO: fall back to cursor simulation if client has not bound to touch
	wlr_seat_touch_notify_up(seat, event->time_msec, event->touch_id);
}

static void handle_touch_motion(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, touch_motion);
	wlr_idle_notify_activity(server.idle, cursor->seat->wlr_seat);
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
	wlr_idle_notify_activity(server.idle, cursor->seat->wlr_seat);
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

	cursor_motion_absolute(cursor, event->time_msec, event->device, x, y);
	wlr_seat_pointer_notify_frame(cursor->seat->wlr_seat);
}

static void handle_tool_tip(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, tool_tip);
	wlr_idle_notify_activity(server.idle, cursor->seat->wlr_seat);
	struct wlr_event_tablet_tool_tip *event = data;
	dispatch_cursor_button(cursor, event->device, event->time_msec,
			BTN_LEFT, event->state == WLR_TABLET_TOOL_TIP_DOWN ?
				WLR_BUTTON_PRESSED : WLR_BUTTON_RELEASED);
	wlr_seat_pointer_notify_frame(cursor->seat->wlr_seat);
	transaction_commit_dirty();
}

static void handle_tool_button(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, tool_button);
	wlr_idle_notify_activity(server.idle, cursor->seat->wlr_seat);
	struct wlr_event_tablet_tool_button *event = data;
	// TODO: the user may want to configure which tool buttons are mapped to
	// which simulated pointer buttons
	switch (event->state) {
	case WLR_BUTTON_PRESSED:
		if (cursor->tool_buttons == 0) {
			dispatch_cursor_button(cursor, event->device,
					event->time_msec, BTN_RIGHT, event->state);
		}
		cursor->tool_buttons++;
		break;
	case WLR_BUTTON_RELEASED:
		if (cursor->tool_buttons == 1) {
			dispatch_cursor_button(cursor, event->device,
					event->time_msec, BTN_RIGHT, event->state);
		}
		cursor->tool_buttons--;
		break;
	}
	wlr_seat_pointer_notify_frame(cursor->seat->wlr_seat);
	transaction_commit_dirty();
}

static void check_constraint_region(struct sway_cursor *cursor) {
	struct wlr_pointer_constraint_v1 *constraint = cursor->active_constraint;
	pixman_region32_t *region = &constraint->region;
	struct sway_view *view = view_from_wlr_surface(constraint->surface);
	if (view) {
		struct sway_container *con = view->container;

		double sx = cursor->cursor->x - con->content_x + view->geometry.x;
		double sy = cursor->cursor->y - con->content_y + view->geometry.y;

		if (!pixman_region32_contains_point(region,
				floor(sx), floor(sy), NULL)) {
			int nboxes;
			pixman_box32_t *boxes = pixman_region32_rectangles(region, &nboxes);
			if (nboxes > 0) {
				double sx = (boxes[0].x1 + boxes[0].x2) / 2.;
				double sy = (boxes[0].y1 + boxes[0].y2) / 2.;

				wlr_cursor_warp_closest(cursor->cursor, NULL,
					sx + con->content_x - view->geometry.x,
					sy + con->content_y - view->geometry.y);
			}
		}
	}

	// A locked pointer will result in an empty region, thus disallowing all movement
	if (constraint->type == WLR_POINTER_CONSTRAINT_V1_CONFINED) {
		pixman_region32_copy(&cursor->confine, region);
	} else {
		pixman_region32_clear(&cursor->confine);
	}
}

static void handle_constraint_commit(struct wl_listener *listener,
		void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, constraint_commit);
	struct wlr_pointer_constraint_v1 *constraint = cursor->active_constraint;
	assert(constraint->surface == data);

	check_constraint_region(cursor);
}

static void handle_request_set_cursor(struct wl_listener *listener,
		void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, request_set_cursor);
	if (seat_doing_seatop(cursor->seat)) {
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
		sway_log(SWAY_DEBUG, "denying request to set cursor from unfocused client");
		return;
	}

	cursor_set_image_surface(cursor, event->surface, event->hotspot_x,
			event->hotspot_y, focused_client);
}

void cursor_set_image(struct sway_cursor *cursor, const char *image,
		struct wl_client *client) {
	if (!(cursor->seat->wlr_seat->capabilities & WL_SEAT_CAPABILITY_POINTER)) {
		return;
	}

	const char *current_image = cursor->image;
	cursor->image = image;
	cursor->image_surface = NULL;
	cursor->hotspot_x = cursor->hotspot_y = 0;
	cursor->image_client = client;

	if (cursor->hidden) {
		return;
	}

	if (!image) {
		wlr_cursor_set_image(cursor->cursor, NULL, 0, 0, 0, 0, 0, 0);
	} else if (!current_image || strcmp(current_image, image) != 0) {
		wlr_xcursor_manager_set_cursor_image(cursor->xcursor_manager, image,
				cursor->cursor);
	}
}

void cursor_set_image_surface(struct sway_cursor *cursor,
		struct wlr_surface *surface, int32_t hotspot_x, int32_t hotspot_y,
		struct wl_client *client) {
	if (!(cursor->seat->wlr_seat->capabilities & WL_SEAT_CAPABILITY_POINTER)) {
		return;
	}

	cursor->image = NULL;
	cursor->image_surface = surface;
	cursor->hotspot_x = hotspot_x;
	cursor->hotspot_y = hotspot_y;
	cursor->image_client = client;

	if (cursor->hidden) {
		return;
	}

	wlr_cursor_set_surface(cursor->cursor, surface, hotspot_x, hotspot_y);
}

void sway_cursor_destroy(struct sway_cursor *cursor) {
	if (!cursor) {
		return;
	}

	wl_event_source_remove(cursor->hide_source);

	wl_list_remove(&cursor->motion.link);
	wl_list_remove(&cursor->motion_absolute.link);
	wl_list_remove(&cursor->button.link);
	wl_list_remove(&cursor->axis.link);
	wl_list_remove(&cursor->frame.link);
	wl_list_remove(&cursor->touch_down.link);
	wl_list_remove(&cursor->touch_up.link);
	wl_list_remove(&cursor->touch_motion.link);
	wl_list_remove(&cursor->tool_axis.link);
	wl_list_remove(&cursor->tool_tip.link);
	wl_list_remove(&cursor->tool_button.link);
	wl_list_remove(&cursor->request_set_cursor.link);

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

	cursor->hide_source = wl_event_loop_add_timer(server.wl_event_loop,
			hide_notify, cursor);

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

	wl_signal_add(&wlr_cursor->events.frame, &cursor->frame);
	cursor->frame.notify = handle_cursor_frame;

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

	wl_list_init(&cursor->constraint_commit.link);

	cursor->cursor = wlr_cursor;

	return cursor;
}

/**
 * Warps the cursor to the middle of the container argument.
 * Does nothing if the cursor is already inside the container.
 * If container is NULL, returns without doing anything.
 */
void cursor_warp_to_container(struct sway_cursor *cursor,
		struct sway_container *container) {
	if (!container) {
		return;
	}

	struct wlr_box box;
	container_get_box(container, &box);
	if (wlr_box_contains_point(&box, cursor->cursor->x, cursor->cursor->y)) {
		return;
	}

	double x = container->x + container->width / 2.0;
	double y = container->y + container->height / 2.0;

	wlr_cursor_warp(cursor->cursor, NULL, x, y);
}

/**
 * Warps the cursor to the middle of the workspace argument.
 * If workspace is NULL, returns without doing anything.
 */
void cursor_warp_to_workspace(struct sway_cursor *cursor,
		struct sway_workspace *workspace) {
	if (!workspace) {
		return;
	}

	double x = workspace->x + workspace->width / 2.0;
	double y = workspace->y + workspace->height / 2.0;

	wlr_cursor_warp(cursor->cursor, NULL, x, y);
}

uint32_t get_mouse_bindsym(const char *name, char **error) {
	if (strncasecmp(name, "button", strlen("button")) == 0) {
		// Map to x11 mouse buttons
		int number = name[strlen("button")] - '0';
		if (number < 1 || number > 9 || strlen(name) > strlen("button0")) {
			*error = strdup("Only buttons 1-9 are supported. For other mouse "
					"buttons, use the name of the event code.");
			return 0;
		}
		static const uint32_t buttons[] = {BTN_LEFT, BTN_MIDDLE, BTN_RIGHT,
			SWAY_SCROLL_UP, SWAY_SCROLL_DOWN, SWAY_SCROLL_LEFT,
			SWAY_SCROLL_RIGHT, BTN_SIDE, BTN_EXTRA};
		return buttons[number - 1];
	} else if (strncmp(name, "BTN_", strlen("BTN_")) == 0) {
		// Get event code from name
		int code = libevdev_event_code_from_name(EV_KEY, name);
		if (code == -1) {
			size_t len = snprintf(NULL, 0, "Unknown event %s", name) + 1;
			*error = malloc(len);
			if (*error) {
				snprintf(*error, len, "Unknown event %s", name);
			}
			return 0;
		}
		return code;
	}
	return 0;
}

uint32_t get_mouse_bindcode(const char *name, char **error) {
	// Validate event code
	errno = 0;
	char *endptr;
	int code = strtol(name, &endptr, 10);
	if (endptr == name && code <= 0) {
		*error = strdup("Button event code must be a positive integer.");
		return 0;
	} else if (errno == ERANGE) {
		*error = strdup("Button event code out of range.");
		return 0;
	}
	const char *event = libevdev_event_code_get_name(EV_KEY, code);
	if (!event || strncmp(event, "BTN_", strlen("BTN_")) != 0) {
		size_t len = snprintf(NULL, 0, "Event code %d (%s) is not a button",
				code, event) + 1;
		*error = malloc(len);
		if (*error) {
			snprintf(*error, len, "Event code %d (%s) is not a button",
					code, event);
		}
		return 0;
	}
	return code;
}

uint32_t get_mouse_button(const char *name, char **error) {
	uint32_t button = get_mouse_bindsym(name, error);
	if (!button && !*error) {
		button = get_mouse_bindcode(name, error);
	}
	return button;
}

const char *get_mouse_button_name(uint32_t button) {
	const char *name = libevdev_event_code_get_name(EV_KEY, button);
	if (!name) {
		if (button == SWAY_SCROLL_UP) {
			name = "SWAY_SCROLL_UP";
		} else if (button == SWAY_SCROLL_DOWN) {
			name = "SWAY_SCROLL_DOWN";
		} else if (button == SWAY_SCROLL_LEFT) {
			name = "SWAY_SCROLL_LEFT";
		} else if (button == SWAY_SCROLL_RIGHT) {
			name = "SWAY_SCROLL_RIGHT";
		}
	}
	return name;
}

static void warp_to_constraint_cursor_hint(struct sway_cursor *cursor) {
	struct wlr_pointer_constraint_v1 *constraint = cursor->active_constraint;

	if (constraint->current.committed &
			WLR_POINTER_CONSTRAINT_V1_STATE_CURSOR_HINT) {
		double sx = constraint->current.cursor_hint.x;
		double sy = constraint->current.cursor_hint.y;

		struct sway_view *view = view_from_wlr_surface(constraint->surface);
		struct sway_container *con = view->container;

		double lx = sx + con->content_x - view->geometry.x;
		double ly = sy + con->content_y - view->geometry.y;

		wlr_cursor_warp(cursor->cursor, NULL, lx, ly);
	}
}

void handle_constraint_destroy(struct wl_listener *listener, void *data) {
	struct sway_pointer_constraint *sway_constraint =
		wl_container_of(listener, sway_constraint, destroy);
	struct wlr_pointer_constraint_v1 *constraint = data;
	struct sway_seat *seat = constraint->seat->data;
	struct sway_cursor *cursor = seat->cursor;

	wl_list_remove(&sway_constraint->destroy.link);

	if (cursor->active_constraint == constraint) {
		warp_to_constraint_cursor_hint(cursor);

		if (cursor->constraint_commit.link.next != NULL) {
			wl_list_remove(&cursor->constraint_commit.link);
		}
		wl_list_init(&cursor->constraint_commit.link);
		cursor->active_constraint = NULL;
	}

	free(sway_constraint);
}

void handle_pointer_constraint(struct wl_listener *listener, void *data) {
	struct wlr_pointer_constraint_v1 *constraint = data;
	struct sway_seat *seat = constraint->seat->data;

	struct sway_pointer_constraint *sway_constraint =
		calloc(1, sizeof(struct sway_pointer_constraint));
	sway_constraint->constraint = constraint;

	sway_constraint->destroy.notify = handle_constraint_destroy;
	wl_signal_add(&constraint->events.destroy, &sway_constraint->destroy);

	struct sway_node *focus = seat_get_focus(seat);
	if (focus && focus->type == N_CONTAINER && focus->sway_container->view) {
		struct wlr_surface *surface = focus->sway_container->view->surface;
		if (surface == constraint->surface) {
			sway_cursor_constrain(seat->cursor, constraint);
		}
	}
}

void sway_cursor_constrain(struct sway_cursor *cursor,
		struct wlr_pointer_constraint_v1 *constraint) {
	struct seat_config *config = seat_get_config(cursor->seat);
	if (!config) {
		config = seat_get_config_by_name("*");
	}

	if (!config || config->allow_constrain == CONSTRAIN_DISABLE) {
		return;
	}

	if (cursor->active_constraint == constraint) {
		return;
	}

	wl_list_remove(&cursor->constraint_commit.link);
	if (cursor->active_constraint) {
		if (constraint == NULL) {
			warp_to_constraint_cursor_hint(cursor);
		}
		wlr_pointer_constraint_v1_send_deactivated(
			cursor->active_constraint);
	}

	cursor->active_constraint = constraint;

	if (constraint == NULL) {
		wl_list_init(&cursor->constraint_commit.link);
		return;
	}

	// FIXME: Big hack, stolen from wlr_pointer_constraints_v1.c:121.
	// This is necessary because the focus may be set before the surface
	// has finished committing, which means that warping won't work properly,
	// since this code will be run *after* the focus has been set.
	// That is why we duplicate the code here.
	if (pixman_region32_not_empty(&constraint->current.region)) {
		pixman_region32_intersect(&constraint->region,
			&constraint->surface->input_region, &constraint->current.region);
	} else {
		pixman_region32_copy(&constraint->region,
			&constraint->surface->input_region);
	}

	check_constraint_region(cursor);

	wlr_pointer_constraint_v1_send_activated(constraint);

	cursor->constraint_commit.notify = handle_constraint_commit;
	wl_signal_add(&constraint->surface->events.commit,
		&cursor->constraint_commit);
}
