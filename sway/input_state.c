#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "input_state.h"

enum { KEY_STATE_MAX_LENGTH = 64 };

static keycode key_state_array[KEY_STATE_MAX_LENGTH];
static uint8_t key_state_length = 0;

static uint8_t find_key(keycode key) {
	int i;
	for (i = 0; i < key_state_length; ++i) {
		if (key_state_array[i] == key) {
			break;
		}
	}
	return i;
}

bool check_key(keycode key) {
	return find_key(key) < key_state_length;
}

void press_key(keycode key) {
	// Check if key exists
	if (!check_key(key)) {
		// Check that we dont exceed buffer length
		if (key_state_length < KEY_STATE_MAX_LENGTH) {
			key_state_array[key_state_length++] = key;
		}
	}
}

void release_key(keycode key) {
	uint8_t index = find_key(key);
	if (index < key_state_length) {
		//shift it over and remove key
		memmove(&key_state_array[index],
				&key_state_array[index + 1],
				sizeof(*key_state_array) * (--key_state_length - index));
	}
}

struct pointer_state pointer_state = {0, 0, {0, 0}, {0, 0, 0, 0}};

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
	pointer_state.floating = (struct pointer_floating){0,0};
	pointer_state.lock = (struct pointer_lock){0,0,0,0};
}

