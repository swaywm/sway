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

enum pointer_mode {
	// Target
	M_FLOATING = 1 << 0,
	M_TILING = 1 << 1,
	// Action
	M_DRAGGING = 1 << 2,
	M_RESIZING = 1 << 3,
};

extern struct pointer_state {
	// mouse clicks
	bool l_held : 1;
	bool r_held : 1;

	// scroll wheel
	bool s_held : 1;
	bool s_up : 1;
	bool s_down :1;

	// pointer position
	struct mouse_origin{
		int x, y;
	} origin;
	struct {
		int x, y;
	} delta;

	// view pointer is over
	swayc_t *view;

	// Pointer mode
	int mode;

	// OLD
	struct pointer_floating {
		bool drag;
		bool resize;
	} floating;
	struct pointer_tiling {
		bool resize;
		swayc_t *init_view;
		struct wlc_origin lock_pos;
	} tiling;
	struct pointer_lock {
		// Lock movement for certain edges
		bool left;
		bool right;
		bool top;
		bool bottom;
		// Lock movement in certain directions
		bool temp_left;
		bool temp_right;
		bool temp_up;
		bool temp_down;
	} lock;
} pointer_state;

// on button release unset mode depending on the button.
// on button press set mode conditionally depending on the button
void pointer_mode_set(uint32_t button, bool condition);

// Update mode in mouse motion
void pointer_mode_update(void);

// Reset mode on any keypress;
void pointer_mode_reset(void);

void start_floating(swayc_t *view);
void reset_floating(swayc_t *view);
void input_init(void);

#endif

