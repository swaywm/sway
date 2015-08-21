#ifndef  _SWAY_KEY_STATE_H
#define  _SWAY_KEY_STATE_H
#include <stdbool.h>
#include <stdint.h>
#include "container.h"

/* Keyboard state */

typedef uint32_t keycode;

// returns true if key has been pressed, otherwise false
bool check_key(keycode key);

// sets a key as pressed
void press_key(keycode key);

// unsets a key as pressed
void release_key(keycode key);

/* Pointer state */

enum pointer_values {
	M_LEFT_CLICK = 272,
	M_RIGHT_CLICK = 273,
	M_SCROLL_CLICK = 274,
	M_SCROLL_UP = 275,
	M_SCROLL_DOWN = 276,
};

extern struct pointer_state {
	bool l_held;
	bool r_held;
	struct pointer_floating {
		bool drag;
		bool resize;
	} floating;
	struct pointer_tiling {
		bool resize;
		swayc_t *init_view;
	} tiling;
	struct pointer_lock {
		bool left;
		bool right;
		bool top;
		bool bottom;
	} lock;
} pointer_state;

void start_floating(swayc_t *view);
void reset_floating(swayc_t *view);

#endif

