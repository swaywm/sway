#define _POSIX_C_SOURCE 200809L
#include <float.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_tablet_v2.h>
#include <wlr/types/wlr_touch.h>
#include "sway/input/cursor.h"
#include "sway/input/seat.h"
#include "sway/tree/view.h"
#include "sway/desktop/transaction.h"
#include "log.h"

struct seatop_touch_point_event {
	double ref_lx, ref_ly;         // touch's x/y at start of op
	double ref_con_lx, ref_con_ly; // container's x/y at start of op
	int32_t touch_id;
	struct wl_list link;
};

struct seatop_down_event {
	struct sway_container *con;
	struct sway_seat *seat;
	struct wl_listener surface_destroy;
	struct wlr_surface *surface;
	double ref_lx, ref_ly;         // cursor's x/y at start of op
	double ref_con_lx, ref_con_ly; // container's x/y at start of op
	struct wl_list point_events;   // seatop_touch_point_event::link
};

static void handle_touch_motion(struct sway_seat *seat,
		struct wlr_touch_motion_event *event, double lx, double ly) {
	struct seatop_down_event *e = seat->seatop_data;

	struct seatop_touch_point_event *point_event;
	bool found = false;
	wl_list_for_each(point_event, &e->point_events, link) {
		if (point_event->touch_id == event->touch_id) {
			found = true;
			break;
		}
	}
	if (!found) {
		return; // Probably not a point_event from this seatop_down
	}

	double moved_x = lx - point_event->ref_lx;
	double moved_y = ly - point_event->ref_ly;
	double sx = point_event->ref_con_lx + moved_x;
	double sy = point_event->ref_con_ly + moved_y;

	wlr_seat_touch_notify_motion(seat->wlr_seat, event->time_msec,
		event->touch_id, sx, sy);
}

static void handle_touch_up(struct sway_seat *seat,
		struct wlr_touch_up_event *event) {
	struct seatop_down_event *e = seat->seatop_data;
	struct seatop_touch_point_event *point_event, *tmp;

	wl_list_for_each_safe(point_event, tmp, &e->point_events, link) {
		if (point_event->touch_id == event->touch_id) {
			wl_list_remove(&point_event->link);
			free(point_event);
			break;
		}
	}

	if (wl_list_empty(&e->point_events)) {
		seatop_begin_default(seat);
	}

	wlr_seat_touch_notify_up(seat->wlr_seat, event->time_msec, event->touch_id);
}

static void handle_touch_down(struct sway_seat *seat,
		struct wlr_touch_down_event *event, double lx, double ly) {
	struct seatop_down_event *e = seat->seatop_data;
	double sx, sy;
	struct wlr_surface *surface = NULL;
	struct sway_node *focused_node = node_at_coords(seat, seat->touch_x,
		seat->touch_y, &surface, &sx, &sy);

	if (!surface || surface != e->surface) { // Must start from the initial surface
		return;
	}

	struct seatop_touch_point_event *point_event =
		calloc(1, sizeof(struct seatop_touch_point_event));
	if (!sway_assert(point_event, "Unable to allocate point_event")) {
		return;
	}
	point_event->touch_id = event->touch_id;
	point_event->ref_lx = lx;
	point_event->ref_ly = ly;
	point_event->ref_con_lx = sx;
	point_event->ref_con_ly = sy;

	wl_list_insert(&e->point_events, &point_event->link);

	wlr_seat_touch_notify_down(seat->wlr_seat, surface, event->time_msec,
			event->touch_id, sx, sy);

	if (focused_node) {
	    seat_set_focus(seat, focused_node);
	}
}

static void handle_pointer_axis(struct sway_seat *seat,
		struct wlr_pointer_axis_event *event) {
	struct sway_input_device *input_device =
		event->pointer ? event->pointer->base.data : NULL;
	struct input_config *ic =
		input_device ? input_device_get_config(input_device) : NULL;
	float scroll_factor =
		(ic == NULL || ic->scroll_factor == FLT_MIN) ? 1.0f : ic->scroll_factor;

	wlr_seat_pointer_notify_axis(seat->wlr_seat, event->time_msec,
		event->orientation, scroll_factor * event->delta,
		roundf(scroll_factor * event->delta_discrete), event->source);
}

static void handle_button(struct sway_seat *seat, uint32_t time_msec,
		struct wlr_input_device *device, uint32_t button,
		enum wlr_button_state state) {
	seat_pointer_notify_button(seat, time_msec, button, state);

	if (seat->cursor->pressed_button_count == 0) {
		seatop_begin_default(seat);
	}
}

static void handle_pointer_motion(struct sway_seat *seat, uint32_t time_msec) {
	struct seatop_down_event *e = seat->seatop_data;
	if (seat_is_input_allowed(seat, e->surface)) {
		double moved_x = seat->cursor->cursor->x - e->ref_lx;
		double moved_y = seat->cursor->cursor->y - e->ref_ly;
		double sx = e->ref_con_lx + moved_x;
		double sy = e->ref_con_ly + moved_y;
		wlr_seat_pointer_notify_motion(seat->wlr_seat, time_msec, sx, sy);
	}
}

static void handle_tablet_tool_tip(struct sway_seat *seat,
		struct sway_tablet_tool *tool, uint32_t time_msec,
		enum wlr_tablet_tool_tip_state state) {
	if (state == WLR_TABLET_TOOL_TIP_UP) {
		wlr_tablet_v2_tablet_tool_notify_up(tool->tablet_v2_tool);
		seatop_begin_default(seat);
	}
}

static void handle_tablet_tool_motion(struct sway_seat *seat,
		struct sway_tablet_tool *tool, uint32_t time_msec) {
	struct seatop_down_event *e = seat->seatop_data;
	if (seat_is_input_allowed(seat, e->surface)) {
		double moved_x = seat->cursor->cursor->x - e->ref_lx;
		double moved_y = seat->cursor->cursor->y - e->ref_ly;
		double sx = e->ref_con_lx + moved_x;
		double sy = e->ref_con_ly + moved_y;
		wlr_tablet_v2_tablet_tool_notify_motion(tool->tablet_v2_tool, sx, sy);
	}
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct seatop_down_event *e =
		wl_container_of(listener, e, surface_destroy);
	if (e) {
		seatop_begin_default(e->seat);
	}
}

static void handle_unref(struct sway_seat *seat, struct sway_container *con) {
	struct seatop_down_event *e = seat->seatop_data;
	if (e->con == con) {
		seatop_begin_default(seat);
	}
}

static void handle_end(struct sway_seat *seat) {
	struct seatop_down_event *e = seat->seatop_data;
	wl_list_remove(&e->surface_destroy.link);
}

static const struct sway_seatop_impl seatop_impl = {
	.button = handle_button,
	.pointer_motion = handle_pointer_motion,
	.pointer_axis = handle_pointer_axis,
	.tablet_tool_tip = handle_tablet_tool_tip,
	.tablet_tool_motion = handle_tablet_tool_motion,
	.touch_motion = handle_touch_motion,
	.touch_up = handle_touch_up,
	.touch_down = handle_touch_down,
	.unref = handle_unref,
	.end = handle_end,
	.allow_set_cursor = true,
};

void seatop_begin_down(struct sway_seat *seat, struct sway_container *con,
		double sx, double sy) {
	seatop_begin_down_on_surface(seat, con->view->surface, sx, sy);
	struct seatop_down_event *e = seat->seatop_data;
	e->con = con;

	container_raise_floating(con);
	transaction_commit_dirty();
}

void seatop_begin_touch_down(struct sway_seat *seat,
		struct wlr_surface *surface, struct wlr_touch_down_event *event,
		double sx, double sy, double lx, double ly) {
	seatop_begin_down_on_surface(seat, surface, sx, sy);
	handle_touch_down(seat, event, lx, ly);
}

void seatop_begin_down_on_surface(struct sway_seat *seat,
		struct wlr_surface *surface, double sx, double sy) {
	seatop_end(seat);

	struct seatop_down_event *e =
		calloc(1, sizeof(struct seatop_down_event));
	if (!sway_assert(e, "Unable to allocate e")) {
		return;
	}
	e->con = NULL;
	e->seat = seat;
	e->surface = surface;
	wl_signal_add(&e->surface->events.destroy, &e->surface_destroy);
	e->surface_destroy.notify = handle_destroy;
	e->ref_lx = seat->cursor->cursor->x;
	e->ref_ly = seat->cursor->cursor->y;
	e->ref_con_lx = sx;
	e->ref_con_ly = sy;
	wl_list_init(&e->point_events);

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;
}
