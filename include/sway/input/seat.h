#ifndef _SWAY_SEAT_H
#define _SWAY_SEAT_H

#include <wlr/types/wlr_seat.h>
#include "sway/input/input-manager.h"

struct sway_seat {
	struct wlr_seat *seat;
	struct sway_cursor *cursor;
	struct sway_input_manager *input;
	swayc_t *focus;

	list_t *devices;

	struct wl_listener focus_destroy;

	struct wl_list link; // input_manager::seats
};

struct sway_seat *sway_seat_create(struct sway_input_manager *input,
		const char *seat_name);

void sway_seat_add_device(struct sway_seat *seat,
		struct sway_input_device *device);

void sway_seat_remove_device(struct sway_seat *seat,
		struct sway_input_device *device);

void sway_seat_configure_xcursor(struct sway_seat *seat);

void sway_seat_set_focus(struct sway_seat *seat, swayc_t *container);

#endif
