#ifndef _SWAY_CURSOR_H
#define _SWAY_CURSOR_H

#include "sway/seat.h"

struct sway_cursor {
	struct wlr_cursor *cursor;

	struct wl_listener motion;
	struct wl_listener motion_absolute;
	struct wl_listener button;
	struct wl_listener axis;

	struct wl_listener touch_down;
	struct wl_listener touch_up;
	struct wl_listener touch_motion;

	struct wl_listener tool_axis;
	struct wl_listener tool_tip;

	struct wl_listener request_set_cursor;
};

struct sway_cursor *sway_cursor_create(struct sway_seat *seat);

#endif
