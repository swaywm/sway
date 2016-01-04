#ifndef  _SWAY_KEY_STATE_H
#define  _SWAY_KEY_STATE_H
#include <stdbool.h>
#include <stdint.h>
#include "container.h"

/* Keyboard state */

// returns true if key has been pressed, otherwise false
bool check_key(uint32_t key_sym, uint32_t key_code);

// sets a key as pressed
void press_key(uint32_t key_sym, uint32_t key_code);

// unsets a key as pressed
void release_key(uint32_t key_sym, uint32_t key_code);


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
	M_FLOATING = 1,
	M_TILING = 2,
	// Action
	M_DRAGGING = 4,
	M_RESIZING = 8,
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

	// change in pointer position
	struct {
		int x, y;
	} delta;

	// view pointer is currently over
	swayc_t *view;

	// Pointer mode
	int mode;
} pointer_state;

enum modifier_state {
	MOD_STATE_UNCHANGED = 0,
	MOD_STATE_PRESSED = 1,
	MOD_STATE_RELEASED = 2
};

void pointer_position_set(struct wlc_origin *new_origin, bool force_focus);
void center_pointer_on(swayc_t *view);

// on button release unset mode depending on the button.
// on button press set mode conditionally depending on the button
void pointer_mode_set(uint32_t button, bool condition);

// Update mode in mouse motion
void pointer_mode_update(void);

// Reset mode on any keypress;
void pointer_mode_reset(void);

void input_init(void);

/**
 * Check if state of mod changed from current state to new_state.
 *
 * Returns MOD_STATE_UNCHANGED if the state didn't change, MOD_STATE_PRESSED if
 * the state changed to pressed and MOD_STATE_RELEASED if the state changed to
 * released.
 */
uint32_t modifier_state_changed(uint32_t new_state, uint32_t mod);

/**
 * Update the current modifiers state to new_state.
 */
void modifiers_state_update(uint32_t new_state);

#endif

