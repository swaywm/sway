#define _XOPEN_SOURCE 700
#include <math.h>
#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_idle.h>
#include "list.h"
#include "log.h"
#include "sway/input/cursor.h"
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"

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
 * Returns the container at the cursor's position. If there is a surface at that
 * location, it is stored in **surface (it may not be a view).
 */
static struct sway_container *container_at_coords(
		struct sway_seat *seat, double x, double y,
		struct wlr_surface **surface, double *sx, double *sy) {
	// check for unmanaged views first
	struct wl_list *unmanaged = &root_container.sway_root->xwayland_unmanaged;
	struct sway_xwayland_unmanaged *unmanaged_surface;
	wl_list_for_each_reverse(unmanaged_surface, unmanaged, link) {
		struct wlr_xwayland_surface *xsurface =
			unmanaged_surface->wlr_xwayland_surface;

		double _sx = x - unmanaged_surface->lx;
		double _sy = y - unmanaged_surface->ly;
		if (wlr_surface_point_accepts_input(xsurface->surface, _sx, _sy)) {
			*surface = xsurface->surface;
			*sx = _sx;
			*sy = _sy;
			return NULL;
		}
	}

	// find the output the cursor is on
	struct wlr_output_layout *output_layout =
		root_container.sway_root->output_layout;
	struct wlr_output *wlr_output = wlr_output_layout_output_at(
			output_layout, x, y);
	if (wlr_output == NULL) {
		return NULL;
	}
	struct sway_output *output = wlr_output->data;
	double ox = x, oy = y;
	wlr_output_layout_output_coords(output_layout, wlr_output, &ox, &oy);

	// find the focused workspace on the output for this seat
	struct sway_container *ws = seat_get_focus_inactive(seat, output->swayc);
	if (ws && ws->type != C_WORKSPACE) {
		ws = container_parent(ws, C_WORKSPACE);
	}
	if (!ws) {
		return output->swayc;
	}

	if ((*surface = layer_surface_at(output,
				&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],
				ox, oy, sx, sy))) {
		return ws;
	}
	if (ws->sway_workspace->fullscreen) {
		struct wlr_surface *wlr_surface = ws->sway_workspace->fullscreen->surface;
		if (wlr_surface_point_accepts_input(wlr_surface, ox, oy)) {
			*sx = ox;
			*sy = oy;
			*surface = wlr_surface;
			return ws->sway_workspace->fullscreen->swayc;
		}
		return NULL;
	}
	if ((*surface = layer_surface_at(output,
				&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP],
				ox, oy, sx, sy))) {
		return ws;
	}

	struct sway_container *c;
	if ((c = container_at(ws, ox, oy, surface, sx, sy))) {
		return c;
	}

	if ((*surface = layer_surface_at(output,
				&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM],
				ox, oy, sx, sy))) {
		return ws;
	}
	if ((*surface = layer_surface_at(output,
				&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND],
				ox, oy, sx, sy))) {
		return ws;
	}

	c = seat_get_focus_inactive(seat, output->swayc);
	if (c) {
		return c;
	}
	if (!c && output->swayc->children->length) {
		c = output->swayc->children->items[0];
		return c;
	}

	return output->swayc;
}

void cursor_send_pointer_motion(struct sway_cursor *cursor, uint32_t time_msec,
		bool allow_refocusing) {
	if (time_msec == 0) {
		time_msec = get_current_time_msec();
	}

	struct wlr_seat *seat = cursor->seat->wlr_seat;
	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct sway_container *c = container_at_coords(cursor->seat,
			cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);
	if (c && config->focus_follows_mouse && allow_refocusing) {
		struct sway_container *focus = seat_get_focus(cursor->seat);
		if (focus && c->type == C_WORKSPACE) {
			// Only follow the mouse if it would move to a new output
			// Otherwise we'll focus the workspace, which is probably wrong
			if (focus->type != C_OUTPUT) {
				focus = container_parent(focus, C_OUTPUT);
			}
			struct sway_container *output = c;
			if (output->type != C_OUTPUT) {
				output = container_parent(c, C_OUTPUT);
			}
			if (output != focus) {
				seat_set_focus_warp(cursor->seat, c, false);
			}
		} else {
			// Don't switch focus on title mouseover for
			// stacked and tabbed layouts
			// If pointed container is in nested containers which are
			// inside tabbed/stacked layout we should skip them
			bool do_mouse_focus = true;
			struct sway_container *p = c->parent;
			struct sway_container *first_tabbed_parent = c->parent;
			while(p) {
				if(p->layout == L_TABBED || p->layout == L_STACKED) {
					do_mouse_focus = false;
					first_tabbed_parent = p;
				}
				p = p->parent;
			}
			if(!do_mouse_focus) {
				struct sway_container *next_focus = seat_get_focus_inactive(
						cursor->seat, first_tabbed_parent);
				if(next_focus) {
					seat_set_focus_warp(cursor->seat, next_focus, false);
				}
			} else {
				seat_set_focus_warp(cursor->seat, c, false);
			}
		}
	}

	// reset cursor if switching between clients
	struct wl_client *client = NULL;
	if (surface != NULL) {
		client = wl_resource_get_client(surface->resource);
	}
	if (client != cursor->image_client) {
		wlr_xcursor_manager_set_cursor_image(cursor->xcursor_manager,
			"left_ptr", cursor->cursor);
		cursor->image_client = client;
	}

	// send pointer enter/leave
	if (surface != NULL) {
		if (seat_is_input_allowed(cursor->seat, surface)) {
			wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
			wlr_seat_pointer_notify_motion(seat, time_msec, sx, sy);
		}
	} else {
		wlr_seat_pointer_clear_focus(seat);
	}
}

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, motion);
	wlr_idle_notify_activity(cursor->seat->input->server->idle, cursor->seat->wlr_seat);
	struct wlr_event_pointer_motion *event = data;
	wlr_cursor_move(cursor->cursor, event->device,
		event->delta_x, event->delta_y);
	cursor_send_pointer_motion(cursor, event->time_msec, true);
}

static void handle_cursor_motion_absolute(
		struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, motion_absolute);
	wlr_idle_notify_activity(cursor->seat->input->server->idle, cursor->seat->wlr_seat);
	struct wlr_event_pointer_motion_absolute *event = data;
	wlr_cursor_warp_absolute(cursor->cursor, event->device, event->x, event->y);
	cursor_send_pointer_motion(cursor, event->time_msec, true);
}

void dispatch_cursor_button(struct sway_cursor *cursor,
		uint32_t time_msec, uint32_t button, enum wlr_button_state state) {
	if (time_msec == 0) {
		time_msec = get_current_time_msec();
	}

	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct sway_container *cont = container_at_coords(cursor->seat,
			cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);
	if (surface && wlr_surface_is_layer_surface(surface)) {
		struct wlr_layer_surface *layer =
			wlr_layer_surface_from_wlr_surface(surface);
		if (layer->current.keyboard_interactive) {
			seat_set_focus_layer(cursor->seat, layer);
			return;
		}
	}
	// Avoid moving keyboard focus from a surface that accepts it to one
	// that does not unless the change would move us to a new workspace.
	//
	// This prevents, for example, losing focus when clicking on swaybar.
	if (surface && cont && cont->type != C_VIEW) {
		struct sway_container *new_ws = cont;
		if (new_ws && new_ws->type != C_WORKSPACE) {
			new_ws = container_parent(new_ws, C_WORKSPACE);
		}
		struct sway_container *old_ws = seat_get_focus(cursor->seat);
		if (old_ws && old_ws->type != C_WORKSPACE) {
			old_ws = container_parent(old_ws, C_WORKSPACE);
		}
		if (new_ws != old_ws) {
			seat_set_focus(cursor->seat, cont);
		}
	} else {
		seat_set_focus(cursor->seat, cont);
	}

	wlr_seat_pointer_notify_button(cursor->seat->wlr_seat,
			time_msec, button, state);
}

static void handle_cursor_button(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, button);
	wlr_idle_notify_activity(cursor->seat->input->server->idle, cursor->seat->wlr_seat);
	struct wlr_event_pointer_button *event = data;
	dispatch_cursor_button(cursor,
			event->time_msec, event->button, event->state);
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

	struct wlr_seat *seat = cursor->seat->wlr_seat;
	struct wlr_surface *surface = NULL;

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(cursor->cursor, event->device,
			event->x, event->y, &lx, &ly);
	double sx, sy;
	container_at_coords(cursor->seat, lx, ly, &surface, &sx, &sy);

	if (!surface) {
		return;
	}

	// TODO: fall back to cursor simulation if client has not bound to touch
	if (seat_is_input_allowed(cursor->seat, surface)) {
		wlr_seat_touch_notify_down(seat, surface, event->time_msec,
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

	struct wlr_seat *seat = cursor->seat->wlr_seat;
	struct wlr_surface *surface = NULL;

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(cursor->cursor, event->device,
			event->x, event->y, &lx, &ly);
	double sx, sy;
	container_at_coords(cursor->seat, lx, ly, &surface, &sx, &sy);

	if (!surface) {
		return;
	}

	// TODO: fall back to cursor simulation if client has not bound to touch
	if (seat_is_input_allowed(cursor->seat, surface)) {
		wlr_seat_touch_notify_motion(
				seat, event->time_msec, event->touch_id, sx, sy);
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
}

static void handle_tool_tip(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, tool_tip);
	wlr_idle_notify_activity(cursor->seat->input->server->idle, cursor->seat->wlr_seat);
	struct wlr_event_tablet_tool_tip *event = data;
	dispatch_cursor_button(cursor, event->time_msec,
			BTN_LEFT, event->state == WLR_TABLET_TOOL_TIP_DOWN ?
				WLR_BUTTON_PRESSED : WLR_BUTTON_RELEASED);
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
}

static void handle_request_set_cursor(struct wl_listener *listener,
		void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, request_set_cursor);
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
		wlr_log(L_DEBUG, "denying request to set cursor from unfocused client");
		return;
	}

	wlr_cursor_set_surface(cursor->cursor, event->surface, event->hotspot_x,
		event->hotspot_y);
	cursor->image_client = focused_client;
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

	cursor->seat = seat;
	wlr_cursor_attach_output_layout(wlr_cursor,
		root_container.sway_root->output_layout);

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
