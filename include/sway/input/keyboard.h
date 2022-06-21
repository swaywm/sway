#ifndef _SWAY_INPUT_KEYBOARD_H
#define _SWAY_INPUT_KEYBOARD_H

#include "sway/input/seat.h"

#define SWAY_KEYBOARD_PRESSED_KEYS_CAP 32

/**
 * Get modifier mask from modifier name.
 *
 * Returns the modifier mask or 0 if the name isn't found.
 */
uint32_t get_modifier_mask_by_name(const char *name);

/**
 * Get modifier name from modifier mask.
 *
 * Returns the modifier name or NULL if it isn't found.
 */
const char *get_modifier_name_by_mask(uint32_t modifier);

/**
 * Get an array of modifier names from modifier_masks
 *
 * Populates the names array and return the number of names added.
 */
int get_modifier_names(const char **names, uint32_t modifier_masks);

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
	uint32_t current_key;
};

struct sway_keyboard {
	struct sway_seat_device *seat_device;
	struct wlr_keyboard *wlr;

	struct xkb_keymap *keymap;
	xkb_layout_index_t effective_layout;

	int32_t repeat_rate;
	int32_t repeat_delay;

	struct wl_listener keyboard_key;
	struct wl_listener keyboard_modifiers;

	struct sway_shortcut_state state_keysyms_translated;
	struct sway_shortcut_state state_keysyms_raw;
	struct sway_shortcut_state state_keycodes;
	struct sway_shortcut_state state_pressed_sent;
	struct sway_binding *held_binding;

	struct wl_event_source *key_repeat_source;
	struct sway_binding *repeat_binding;
};

struct sway_keyboard_group {
	struct wlr_keyboard_group *wlr_group;
	struct sway_seat_device *seat_device;
	struct wl_listener keyboard_key;
	struct wl_listener keyboard_modifiers;
	struct wl_listener enter;
	struct wl_listener leave;
	struct wl_list link; // sway_seat::keyboard_groups
};

struct xkb_keymap *sway_keyboard_compile_keymap(struct input_config *ic,
		char **error);

struct sway_keyboard *sway_keyboard_create(struct sway_seat *seat,
		struct sway_seat_device *device);

void sway_keyboard_configure(struct sway_keyboard *keyboard);

void sway_keyboard_destroy(struct sway_keyboard *keyboard);

void sway_keyboard_disarm_key_repeat(struct sway_keyboard *keyboard);
#endif
