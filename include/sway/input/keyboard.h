#ifndef _SWAY_INPUT_KEYBOARD_H
#define _SWAY_INPUT_KEYBOARD_H

#include "sway/input/seat.h"

#define SWAY_KEYBOARD_PRESSED_KEYS_CAP 32

struct sway_shortcut_state {
	uint32_t pressed_keys[SWAY_KEYBOARD_PRESSED_KEYS_CAP];
	uint32_t pressed_keycodes[SWAY_KEYBOARD_PRESSED_KEYS_CAP];
	int last_key_index;
};

struct sway_keyboard {
	struct sway_seat_device *seat_device;

	struct xkb_keymap *keymap;

	struct wl_listener keyboard_key;
	struct wl_listener keyboard_modifiers;

	struct sway_shortcut_state state_keysyms_translated;
	struct sway_shortcut_state state_keysyms_raw;
	struct sway_shortcut_state state_keycodes;
	struct sway_binding *held_binding;
	uint32_t last_modifiers;
};

struct sway_keyboard *sway_keyboard_create(struct sway_seat *seat,
		struct sway_seat_device *device);

void sway_keyboard_configure(struct sway_keyboard *keyboard);

void sway_keyboard_destroy(struct sway_keyboard *keyboard);

#endif
