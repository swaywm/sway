#ifndef _SWAY_INPUT_CURSOR_H
#define _SWAY_INPUT_CURSOR_H
#include <stdint.h>
#include <stdbool.h>
#include <wlr/types/wlr_surface.h>
#include "sway/input/seat.h"

struct sway_cursor {
	struct sway_seat *seat;
	struct wlr_cursor *cursor;
	struct wlr_xcursor_manager *xcursor_manager;

	struct wl_client *image_client;
	struct wlr_surface *image_surface;
	int hotspot_x, hotspot_y;

	struct wl_listener motion;
	struct wl_listener motion_absolute;
	struct wl_listener button;
	struct wl_listener axis;

	struct wl_listener touch_down;
	struct wl_listener touch_up;
	struct wl_listener touch_motion;

	struct wl_listener tool_axis;
	struct wl_listener tool_tip;
	struct wl_listener tool_button;
	uint32_t tool_buttons;

	struct wl_listener request_set_cursor;

	struct wl_event_source *hide_source;
	bool hidden;
};

void sway_cursor_destroy(struct sway_cursor *cursor);
struct sway_cursor *sway_cursor_create(struct sway_seat *seat);
void cursor_send_pointer_motion(struct sway_cursor *cursor, uint32_t time_msec);
void dispatch_cursor_button(struct sway_cursor *cursor, uint32_t time_msec,
	uint32_t button, enum wlr_button_state state);

#endif
