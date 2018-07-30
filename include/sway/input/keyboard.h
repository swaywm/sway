#ifndef _SWAY_INPUT_KEYBOARD_H
#define _SWAY_INPUT_KEYBOARD_H

#include "sway/input/seat.h"

#define SWAY_KEYBOARD_PRESSED_KEYS_CAP 32

struct sway_shortcut_state {
	/**
	 * A list of pressed key ids (either keysyms or keycodes),
	 * including duplicates when different keycodes produce the same key id.
	 *
	 * Each key id is associated with the keycode (in `pressed_keycodes`)
	 * whose press generated it, so that the key id can be removed on
	 * keycode release without recalculating the transient link between
	 * keycode and key id at the time of the key press.
	 */
	uint32_t pressed_keys[SWAY_KEYBOARD_PRESSED_KEYS_CAP];
	/**
	 * The list of keycodes associated to currently pressed key ids,
	 * including duplicates when a keycode generates multiple key ids.
	 */
	uint32_t pressed_keycodes[SWAY_KEYBOARD_PRESSED_KEYS_CAP];
	uint32_t last_keycode;
	uint32_t last_raw_modifiers;
	size_t npressed;
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

	struct wl_event_source *key_repeat_source;
	struct sway_binding *repeat_binding;
};

struct sway_keyboard *sway_keyboard_create(struct sway_seat *seat,
		struct sway_seat_device *device);

void sway_keyboard_configure(struct sway_keyboard *keyboard);

void sway_keyboard_destroy(struct sway_keyboard *keyboard);

#endif
