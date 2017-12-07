#ifndef _SWAY_SEAT_H
#define _SWAY_SEAT_H

#include <wlr/types/wlr_seat.h>
#include "sway/input-manager.h"

struct sway_seat {
	struct wlr_seat *seat;
};

struct sway_seat *sway_seat_create(struct wl_display *display,
		const char *seat_name);

void sway_seat_add_device(struct sway_seat *seat,
		struct wlr_input_device *device);

void sway_seat_remove_device(struct sway_seat *seat,
		struct wlr_input_device *device);

#endif
