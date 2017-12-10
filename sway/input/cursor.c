#define _XOPEN_SOURCE 700
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include "sway/input/cursor.h"
#include "sway/view.h"
#include "list.h"
#include "log.h"

static void cursor_update_position(struct sway_cursor *cursor) {
	double x = cursor->cursor->x;
	double y = cursor->cursor->y;

	wlr_xcursor_manager_set_cursor_image(cursor->xcursor_manager,
		"left_ptr", cursor->cursor);

	cursor->x = x;
	cursor->y = y;
}

static void cursor_send_pointer_motion(struct sway_cursor *cursor,
		uint32_t time) {
	struct wlr_seat *seat = cursor->seat->seat;
	struct wlr_surface *surface = NULL;
	double sx, sy;
	swayc_t *swayc =
		swayc_at(&root_container, cursor->x, cursor->y, &surface, &sx, &sy);
	if (swayc) {
		wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
		wlr_seat_pointer_notify_motion(seat, time, sx, sy);
	} else {
		wlr_seat_pointer_clear_focus(seat);
	}
}

static void handle_cursor_motion(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, motion);
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
	wlr_cursor_warp_absolute(cursor->cursor, event->device,
		event->x_mm / event->width_mm, event->y_mm / event->height_mm);
	cursor_update_position(cursor);
	cursor_send_pointer_motion(cursor, event->time_msec);
}

static void handle_cursor_button(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, button);
	struct wlr_event_pointer_button *event = data;
	wlr_seat_pointer_notify_button(cursor->seat->seat, event->time_msec,
		event->button, event->state);
}

static void handle_cursor_axis(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, axis);
	struct wlr_event_pointer_axis *event = data;
	wlr_seat_pointer_notify_axis(cursor->seat->seat, event->time_msec,
		event->orientation, event->delta);
}

static void handle_touch_down(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, touch_down);
	struct wlr_event_touch_down *event = data;
	sway_log(L_DEBUG, "TODO: handle touch down event: %p", event);
}

static void handle_touch_up(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, touch_up);
	struct wlr_event_touch_up *event = data;
	sway_log(L_DEBUG, "TODO: handle touch up event: %p", event);
}

static void handle_touch_motion(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, touch_motion);
	struct wlr_event_touch_motion *event = data;
	sway_log(L_DEBUG, "TODO: handle touch motion event: %p", event);
}

static void handle_tool_axis(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, tool_axis);
	struct wlr_event_tablet_tool_axis *event = data;
	sway_log(L_DEBUG, "TODO: handle tool axis event: %p", event);
}

static void handle_tool_tip(struct wl_listener *listener, void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, tool_tip);
	struct wlr_event_tablet_tool_tip *event = data;
	sway_log(L_DEBUG, "TODO: handle tool tip event: %p", event);
}

static void handle_request_set_cursor(struct wl_listener *listener,
		void *data) {
	struct sway_cursor *cursor =
		wl_container_of(listener, cursor, request_set_cursor);
	struct wlr_seat_pointer_request_set_cursor_event *event = data;
	sway_log(L_DEBUG, "TODO: handle request set cursor event: %p", event);
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
	wlr_cursor_attach_output_layout(wlr_cursor, root_container.output_layout);

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

	wl_signal_add(&seat->seat->events.request_set_cursor,
			&cursor->request_set_cursor);
	cursor->request_set_cursor.notify = handle_request_set_cursor;

	cursor->cursor = wlr_cursor;

	return cursor;
}
