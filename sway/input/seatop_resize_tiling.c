#define _POSIX_C_SOURCE 200809L
#include <wlr/types/wlr_cursor.h>
#include "sway/commands.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"

struct seatop_resize_tiling_event {
	struct sway_container *con;
	enum wlr_edges edge;
	double ref_lx, ref_ly;         // cursor's x/y at start of op
	double ref_width, ref_height;  // container's size at start of op
	double ref_con_lx, ref_con_ly; // container's x/y at start of op
};

static void handle_motion(struct sway_seat *seat, uint32_t time_msec) {
	struct seatop_resize_tiling_event *e = seat->seatop_data;
	int amount_x = 0;
	int amount_y = 0;
	int moved_x = seat->cursor->cursor->x - e->ref_lx;
	int moved_y = seat->cursor->cursor->y - e->ref_ly;
	enum wlr_edges edge_x = WLR_EDGE_NONE;
	enum wlr_edges edge_y = WLR_EDGE_NONE;
	struct sway_container *con = e->con;

	if (e->edge & WLR_EDGE_TOP) {
		amount_y = (e->ref_height - moved_y) - con->height;
		edge_y = WLR_EDGE_TOP;
	} else if (e->edge & WLR_EDGE_BOTTOM) {
		amount_y = (e->ref_height + moved_y) - con->height;
		edge_y = WLR_EDGE_BOTTOM;
	}
	if (e->edge & WLR_EDGE_LEFT) {
		amount_x = (e->ref_width - moved_x) - con->width;
		edge_x = WLR_EDGE_LEFT;
	} else if (e->edge & WLR_EDGE_RIGHT) {
		amount_x = (e->ref_width + moved_x) - con->width;
		edge_x = WLR_EDGE_RIGHT;
	}

	if (amount_x != 0) {
		container_resize_tiled(e->con, edge_x, amount_x);
	}
	if (amount_y != 0) {
		container_resize_tiled(e->con, edge_y, amount_y);
	}
}

static void handle_finish(struct sway_seat *seat) {
	cursor_set_image(seat->cursor, "left_ptr", NULL);
}

static void handle_abort(struct sway_seat *seat) {
	cursor_set_image(seat->cursor, "left_ptr", NULL);
}

static void handle_unref(struct sway_seat *seat, struct sway_container *con) {
	struct seatop_resize_tiling_event *e = seat->seatop_data;
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

void seatop_begin_resize_tiling(struct sway_seat *seat,
		struct sway_container *con, uint32_t button, enum wlr_edges edge) {
	seatop_abort(seat);

	struct seatop_resize_tiling_event *e =
		calloc(1, sizeof(struct seatop_resize_tiling_event));
	if (!e) {
		return;
	}
	e->con = con;
	e->edge = edge;

	e->ref_lx = seat->cursor->cursor->x;
	e->ref_ly = seat->cursor->cursor->y;
	e->ref_con_lx = con->x;
	e->ref_con_ly = con->y;
	e->ref_width = con->width;
	e->ref_height = con->height;

	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;
	seat->seatop_button = button;
}
