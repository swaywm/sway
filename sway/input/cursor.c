#define _XOPEN_SOURCE 700
#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include "list.h"
#include "log.h"
#include "sway/input/cursor.h"
#include "sway/output.h"
#include "sway/tree/view.h"

static void cursor_update_position(struct sway_cursor *cursor) {
	double x = cursor->cursor->x;
	double y = cursor->cursor->y;

	cursor->x = x;
	cursor->y = y;
}

/**
 * Returns the container at the cursor's position. If the container is a view,
 * stores the surface at the cursor's position in `*surface`.
 */
static struct sway_container *container_at_cursor(struct sway_cursor *cursor,
		struct wlr_surface **surface, double *sx, double *sy) {
	// check for unmanaged views first
	struct wl_list *unmanaged = &root_container.sway_root->unmanaged_views;
	struct sway_view *view;
	wl_list_for_each_reverse(view, unmanaged, unmanaged_view_link) {
		if (view->type == SWAY_XWAYLAND_VIEW) {
			struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
			struct wlr_box box = {
				.x = xsurface->x,
				.y = xsurface->y,
				.width = xsurface->width,
				.height = xsurface->height,
			};

			if (wlr_box_contains_point(&box, cursor->x, cursor->y)) {
				*surface = xsurface->surface;
				*sx = cursor->x - box.x;
				*sy = cursor->y - box.y;
				return view->swayc;
			}
		}
	}

	// find the output the cursor is on
	struct wlr_output_layout *output_layout =
		root_container.sway_root->output_layout;
	struct wlr_output *wlr_output =
		wlr_output_layout_output_at(output_layout, cursor->x, cursor->y);
	if (wlr_output == NULL) {
		return NULL;
	}
	struct sway_output *output = wlr_output->data;

	// find the focused workspace on the output for this seat
	struct sway_container *workspace_cont =
		sway_seat_get_focus_inactive(cursor->seat, output->swayc);
	if (workspace_cont != NULL && workspace_cont->type != C_WORKSPACE) {
		workspace_cont = container_parent(workspace_cont, C_WORKSPACE);
	}
	if (workspace_cont == NULL) {
		return output->swayc;
	}

	struct sway_container *view_cont = container_at(workspace_cont,
		cursor->x, cursor->y, surface, sx, sy);
	return view_cont != NULL ? view_cont : workspace_cont;
}

static void cursor_send_pointer_motion(struct sway_cursor *cursor,
		uint32_t time) {
	struct wlr_seat *seat = cursor->seat->wlr_seat;
	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct sway_container *cont =
		container_at_cursor(cursor, &surface, &sx, &sy);

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
	if (cont != NULL && surface != NULL) {
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(seat, time, sx, sy);
	} else {
		wlr_seat_pointer_clear_focus(seat);
	}
}

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, motion);
	struct wlr_event_pointer_motion *event = data;
	wlr_cursor_move(cursor->cursor, event->device,
		event->delta_x, event->delta_y);
	cursor_update_position(cursor);
	cursor_send_pointer_motion(cursor, event->time_msec);
}

static void handle_cursor_motion_absolute(struct wl_listener *listener,
		void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, motion_absolute);
	struct wlr_event_pointer_motion_absolute *event = data;
	wlr_cursor_warp_absolute(cursor->cursor, event->device, event->x, event->y);
	cursor_update_position(cursor);
	cursor_send_pointer_motion(cursor, event->time_msec);
}

static void handle_cursor_button(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, button);
	struct wlr_event_pointer_button *event = data;

	if (event->button == BTN_LEFT) {
		struct wlr_surface *surface = NULL;
		double sx, sy;
		struct sway_container *cont =
			container_at_cursor(cursor, &surface, &sx, &sy);
		sway_seat_set_focus(cursor->seat, cont);
	}

	wlr_seat_pointer_notify_button(cursor->seat->wlr_seat, event->time_msec,
		event->button, event->state);
}

static void handle_cursor_axis(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, axis);
	struct wlr_event_pointer_axis *event = data;
	wlr_seat_pointer_notify_axis(cursor->seat->wlr_seat, event->time_msec,
		event->orientation, event->delta);
}

static void handle_touch_down(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, touch_down);
	struct wlr_event_touch_down *event = data;
	wlr_log(L_DEBUG, "TODO: handle touch down event: %p", event);
}

static void handle_touch_up(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, touch_up);
	struct wlr_event_touch_up *event = data;
	wlr_log(L_DEBUG, "TODO: handle touch up event: %p", event);
}

static void handle_touch_motion(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, touch_motion);
	struct wlr_event_touch_motion *event = data;
	wlr_log(L_DEBUG, "TODO: handle touch motion event: %p", event);
}

static void handle_tool_axis(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, tool_axis);
	struct wlr_event_tablet_tool_axis *event = data;
	wlr_log(L_DEBUG, "TODO: handle tool axis event: %p", event);
}

static void handle_tool_tip(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor = wl_container_of(listener, cursor, tool_tip);
	struct wlr_event_tablet_tool_tip *event = data;
	wlr_log(L_DEBUG, "TODO: handle tool tip event: %p", event);
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

	wl_signal_add(&wlr_cursor->events.tablet_tool_axis,
		&cursor->tool_axis);
	cursor->tool_axis.notify = handle_tool_axis;

	wl_signal_add(&wlr_cursor->events.tablet_tool_tip, &cursor->tool_tip);
	cursor->tool_tip.notify = handle_tool_tip;

	wl_signal_add(&seat->wlr_seat->events.request_set_cursor,
			&cursor->request_set_cursor);
	cursor->request_set_cursor.notify = handle_request_set_cursor;

	cursor->cursor = wlr_cursor;

	return cursor;
}
