#ifndef _SWAY_INPUT_KEYBOARD_H
#define _SWAY_INPUT_KEYBOARD_H

#include "sway/input/seat.h"

#define SWAY_KEYBOARD_PRESSED_KEYSYMS_CAP 32

/**
 * Get modifier mask from modifier name.
 *
 * Returns the modifer mask or 0 if the name isn't found.
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
