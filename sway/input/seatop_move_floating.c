#define _POSIX_C_SOURCE 200809L
#include <wlr/types/wlr_cursor.h>
#include "sway/desktop.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"

struct seatop_move_floating_event {
	struct sway_container *con;
	double dx, dy; // cursor offset in container
};

static void handle_button(struct sway_seat *seat, uint32_t time_msec,
		struct wlr_input_device *device, uint32_t button,
		enum wlr_button_state state) {
	if (seat->cursor->pressed_button_count == 0) {
		struct seatop_move_floating_event *e = seat->seatop_data;

		// We "move" the container to its own location
		// so it discovers its output again.
		container_floating_move_to(e->con, e->con->x, e->con->y);

		seatop_begin_default(seat);
	}
}

static void handle_motion(struct sway_seat *seat, uint32_t time_msec,
		double dx, double dy) {
	struct seatop_move_floating_event *e = seat->seatop_data;
	struct wlr_cursor *cursor = seat->cursor->cursor;
	desktop_damage_whole_container(e->con);
	container_floating_move_to(e->con, cursor->x - e->dx, cursor->y - e->dy);
	desktop_damage_whole_container(e->con);
}

static void handle_unref(struct sway_seat *seat, struct sway_container *con) {
	struct seatop_move_floating_event *e = seat->seatop_data;
	if (e->con == con) {
		seatop_begin_default(seat);
	}
}

static const struct sway_seatop_impl seatop_impl = {
	.button = handle_button,
	.motion = handle_motion,
	.unref = handle_unref,
};

void seatop_begin_move_floating(struct sway_seat *seat,
		struct sway_container *con) {
	seatop_end(seat);

	struct sway_cursor *cursor = seat->cursor;
	struct seatop_move_floating_event *e =
		calloc(1, sizeof(struct seatop_move_floating_event));
	if (!e) {
		return;
	}
	e->con = con;
	e->dx = cursor->cursor->x - con->x;
	e->dy = cursor->cursor->y - con->y;

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;

	container_raise_floating(con);

	cursor_set_image(cursor, "grab", NULL);
	wlr_seat_pointer_clear_focus(seat->wlr_seat);
}
