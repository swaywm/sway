#define _POSIX_C_SOURCE 200809L
#include <wlr/types/wlr_cursor.h>
#include "sway/input/cursor.h"
#include "sway/input/seat.h"
#include "sway/tree/view.h"

struct seatop_down_event {
	struct sway_container *con;
	double ref_lx, ref_ly;         // cursor's x/y at start of op
	double ref_con_lx, ref_con_ly; // container's x/y at start of op
	bool moved;
};

static void handle_motion(struct sway_seat *seat, uint32_t time_msec) {
	struct seatop_down_event *e = seat->seatop_data;
	struct sway_container *con = e->con;
	if (seat_is_input_allowed(seat, con->view->surface)) {
		double moved_x = seat->cursor->cursor->x - e->ref_lx;
		double moved_y = seat->cursor->cursor->y - e->ref_ly;
		double sx = e->ref_con_lx + moved_x;
		double sy = e->ref_con_ly + moved_y;
		wlr_seat_pointer_notify_motion(seat->wlr_seat, time_msec, sx, sy);
	}
	e->moved = true;
}

static void handle_finish(struct sway_seat *seat) {
	struct seatop_down_event *e = seat->seatop_data;
	struct sway_cursor *cursor = seat->cursor;
	// Set the cursor's previous coords to the x/y at the start of the
	// operation, so the container change will be detected if using
	// focus_follows_mouse and the cursor moved off the original container
	// during the operation.
	cursor->previous.x = e->ref_lx;
	cursor->previous.y = e->ref_ly;
	if (e->moved) {
		struct wlr_surface *surface = NULL;
		double sx, sy;
		struct sway_node *node = node_at_coords(seat,
				cursor->cursor->x, cursor->cursor->y, &surface, &sx, &sy);
		cursor_send_pointer_motion(cursor, 0, node, surface, sx, sy);
	}
}

static void handle_abort(struct sway_seat *seat) {
	cursor_set_image(seat->cursor, "left_ptr", NULL);
}

static void handle_unref(struct sway_seat *seat, struct sway_container *con) {
	struct seatop_down_event *e = seat->seatop_data;
	if (e->con == con) {
		seatop_abort(seat);
	}
}

static const struct sway_seatop_impl seatop_impl = {
	.motion = handle_motion,
	.finish = handle_finish,
	.abort = handle_abort,
	.unref = handle_unref,
};

void seatop_begin_down(struct sway_seat *seat,
		struct sway_container *con, uint32_t button, int sx, int sy) {
	seatop_abort(seat);

	struct seatop_down_event *e =
		calloc(1, sizeof(struct seatop_down_event));
	if (!e) {
		return;
	}
	e->con = con;
	e->ref_lx = seat->cursor->cursor->x;
	e->ref_ly = seat->cursor->cursor->y;
	e->ref_con_lx = sx;
	e->ref_con_ly = sy;
	e->moved = false;

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;
	seat->seatop_button = button;

	container_raise_floating(con);
}
