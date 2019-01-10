#define _POSIX_C_SOURCE 200809L
#include <wlr/types/wlr_cursor.h>
#include "sway/desktop.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"

struct seatop_move_floating_event {
	struct sway_container *con;
};

static void handle_motion(struct sway_seat *seat, uint32_t time_msec) {
	struct seatop_move_floating_event *e = seat->seatop_data;
	desktop_damage_whole_container(e->con);
	container_floating_translate(e->con,
			seat->cursor->cursor->x - seat->cursor->previous.x,
			seat->cursor->cursor->y - seat->cursor->previous.y);
	desktop_damage_whole_container(e->con);
}

static void handle_finish(struct sway_seat *seat) {
	struct seatop_move_floating_event *e = seat->seatop_data;

	// We "move" the container to its own location
	// so it discovers its output again.
	container_floating_move_to(e->con, e->con->x, e->con->y);
	cursor_set_image(seat->cursor, "left_ptr", NULL);
}

static void handle_abort(struct sway_seat *seat) {
	cursor_set_image(seat->cursor, "left_ptr", NULL);
}

static void handle_unref(struct sway_seat *seat, struct sway_container *con) {
	struct seatop_move_floating_event *e = seat->seatop_data;
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

void seatop_begin_move_floating(struct sway_seat *seat,
		struct sway_container *con, uint32_t button) {
	seatop_abort(seat);

	struct seatop_move_floating_event *e =
		calloc(1, sizeof(struct seatop_move_floating_event));
	if (!e) {
		return;
	}
	e->con = con;

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;
	seat->seatop_button = button;

	container_raise_floating(con);

	cursor_set_image(seat->cursor, "grab", NULL);
}
