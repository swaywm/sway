#ifndef _SWAY_INPUT_KEYBOARD_H
#define _SWAY_INPUT_KEYBOARD_H

#include "sway/input/seat.h"

struct sway_keyboard {
	struct sway_seat_device *seat_device;
	struct wl_list link; // sway_seat::keyboards

	struct xkb_keymap *keymap;

	struct wl_listener keyboard_key;
	struct wl_listener keyboard_modifiers;
};

struct sway_keyboard *sway_keyboard_create(struct sway_seat *seat,
		struct sway_seat_device *device);

void sway_keyboard_configure(struct sway_keyboard *keyboard);

void sway_keyboard_destroy(struct sway_keyboard *keyboard);

#endif
