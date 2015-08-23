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

struct pointer_button_state {
	bool held;
	// state at the point it was pressed
	int x, y;
	swayc_t *view;
};

extern struct pointer_state {
	// mouse clicks
	struct pointer_button_state left;
	struct pointer_button_state right;
	struct pointer_button_state scroll;

	// pointer position
	struct mouse_origin{
		int x, y;
	} origin;

	// change in pointer position
	struct {
		int x, y;
	} delta;

	// view pointer is currently over
	swayc_t *view;

	// Pointer mode
	int mode;
} pointer_state;

// on button release unset mode depending on the button.
// on button press set mode conditionally depending on the button
void pointer_mode_set(uint32_t button, bool condition);

// Update mode in mouse motion
void pointer_mode_update(void);

// Reset mode on any keypress;
void pointer_mode_reset(void);

void input_init(void);

#endif

