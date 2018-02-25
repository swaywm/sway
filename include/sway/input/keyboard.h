#ifndef _SWAY_INPUT_KEYBOARD_H
#define _SWAY_INPUT_KEYBOARD_H

#include "sway/input/seat.h"

#define SWAY_KEYBOARD_PRESSED_KEYSYMS_CAP 32

struct sway_keyboard {
	struct sway_seat_device *seat_device;

	struct xkb_keymap *keymap;

	struct wl_listener keyboard_key;
	struct wl_listener keyboard_modifiers;

	xkb_keysym_t pressed_keysyms_translated[SWAY_KEYBOARD_PRESSED_KEYSYMS_CAP];
	uint32_t modifiers_translated;

	xkb_keysym_t pressed_keysyms_raw[SWAY_KEYBOARD_PRESSED_KEYSYMS_CAP];
	uint32_t modifiers_raw;
};

struct sway_keyboard *sway_keyboard_create(struct sway_seat *seat,
		struct sway_seat_device *device);

void sway_keyboard_configure(struct sway_keyboard *keyboard);

void sway_keyboard_destroy(struct sway_keyboard *keyboard);

#endif
