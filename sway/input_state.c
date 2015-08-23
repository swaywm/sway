#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "log.h"

#include "input_state.h"

#define KEY_STATE_MAX_LENGTH 64

struct key_state {
	/*
	 * Aims to store state regardless of modifiers.
	 * If you press a key, then hold shift, then release the key, we'll
	 * get two different key syms, but the same key code. This handles
	 * that scenario and makes sure we can use the right bindings.
	 */
	uint32_t key_sym;
	uint32_t alt_sym;
	uint32_t key_code;
};

static struct key_state key_state_array[KEY_STATE_MAX_LENGTH];

void input_init(void) {
	int i;
	for (i = 0; i < KEY_STATE_MAX_LENGTH; ++i) {
		struct key_state none = { 0, 0, 0 };
		key_state_array[i] = none;
	}
}

static uint8_t find_key(uint32_t key_sym, uint32_t key_code, bool update) {
	int i;
	for (i = 0; i < KEY_STATE_MAX_LENGTH; ++i) {
		if (0 == key_sym && 0 == key_code && key_state_array[i].key_sym == 0) {
			break;
		}
		if (key_state_array[i].key_sym == key_sym
			|| key_state_array[i].alt_sym == key_sym) {
			break;
		}
		if (update && key_state_array[i].key_code == key_code) {
			key_state_array[i].alt_sym = key_sym;
			break;
		}
	}
	return i;
}

bool check_key(uint32_t key_sym, uint32_t key_code) {
	return find_key(key_sym, key_code, false) < KEY_STATE_MAX_LENGTH;
}

void press_key(uint32_t key_sym, uint32_t key_code) {
	if (key_code == 0) {
		return;
	}
	// Check if key exists
	if (!check_key(key_sym, key_code)) {
		// Check that we dont exceed buffer length
		int insert = find_key(0, 0, true);
		if (insert < KEY_STATE_MAX_LENGTH) {
			key_state_array[insert].key_sym = key_sym;
			key_state_array[insert].key_code = key_code;
		}
	}
}

void release_key(uint32_t key_sym, uint32_t key_code) {
	uint8_t index = find_key(key_sym, key_code, true);
	if (index < KEY_STATE_MAX_LENGTH) {
		struct key_state none = { 0, 0, 0 };
		key_state_array[index] = none;
	}
}

struct pointer_state pointer_state;

static struct wlc_geometry saved_floating;

void start_floating(swayc_t *view) {
	if (view->is_floating) {
		saved_floating.origin.x = view->x;
		saved_floating.origin.y = view->y;
		saved_floating.size.w = view->width;
		saved_floating.size.h = view->height;
	}
}

void reset_floating(swayc_t *view) {
	if (view->is_floating) {
		view->x = saved_floating.origin.x;
		view->y = saved_floating.origin.y;
		view->width = saved_floating.size.w;
		view->height = saved_floating.size.h;
		arrange_windows(view->parent, -1, -1);
	}
	pointer_state.floating = (struct pointer_floating){0, 0};
	pointer_state.lock = (struct pointer_lock){0, 0, 0, 0, 0, 0, 0, 0};
}
