#ifndef _SWAY_INPUT_SEAT_H
#define _SWAY_INPUT_SEAT_H

#include <wlr/types/wlr_seat.h>
#include "sway/input/input-manager.h"

struct sway_seat_device {
	struct sway_seat *sway_seat;
	struct sway_input_device *input_device;
	struct sway_keyboard *keyboard;
	struct seat_attachment_config *attachment_config;
	struct wl_list link; // sway_seat::devices
};

struct sway_seat {
	struct wlr_seat *wlr_seat;
	struct seat_config *config;
	struct sway_cursor *cursor;
	struct sway_input_manager *input;
	swayc_t *focus;

	struct wl_listener focus_destroy;

	struct wl_list devices; // sway_seat_device::link

	struct wl_list link; // input_manager::seats
};

struct sway_seat *sway_seat_create(struct sway_input_manager *input,
		const char *seat_name);

void sway_seat_destroy(struct sway_seat *seat);

void sway_seat_add_device(struct sway_seat *seat,
		struct sway_input_device *device);

void sway_seat_configure_device(struct sway_seat *seat,
		struct sway_input_device *device);

void sway_seat_remove_device(struct sway_seat *seat,
		struct sway_input_device *device);

void sway_seat_configure_xcursor(struct sway_seat *seat);

void sway_seat_set_focus(struct sway_seat *seat, swayc_t *container);

void sway_seat_set_config(struct sway_seat *seat, struct seat_config *seat_config);

#endif
