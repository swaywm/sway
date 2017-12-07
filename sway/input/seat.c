#define _XOPEN_SOURCE 700
#include "sway/seat.h"
#include "sway/input-manager.h"
#include "log.h"

struct sway_seat *sway_seat_create(struct wl_display *display,
		const char *seat_name) {
	struct sway_seat *seat = calloc(1, sizeof(struct sway_seat));
	if (!seat) {
		return NULL;
	}
	seat->seat = wlr_seat_create(display, seat_name);
	return seat;
}

void sway_seat_add_device(struct sway_seat *seat,
		struct wlr_input_device *device) {
	sway_log(L_DEBUG, "input add: %s", device->name);
}

void sway_seat_remove_device(struct sway_seat *seat,
		struct wlr_input_device *device) {
	sway_log(L_DEBUG, "input remove: %s", device->name);
}
