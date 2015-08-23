#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "input_state.h"

#define KEY_STATE_MAX_LENGTH 64

static keycode key_state_array[KEY_STATE_MAX_LENGTH];

void input_init(void) {
	int i;
	for (i = 0; i < KEY_STATE_MAX_LENGTH; ++i) {
		key_state_array[i] = 0;
	}
}

static uint8_t find_key(keycode key) {
	int i;
	for (i = 0; i < KEY_STATE_MAX_LENGTH; ++i) {
		if (key_state_array[i] == key) {
			break;
		}
	}
	return i;
}

bool check_key(keycode key) {
	return find_key(key) < KEY_STATE_MAX_LENGTH;
}

void press_key(keycode key) {
	// Check if key exists
	if (!check_key(key)) {
		// Check that we dont exceed buffer length
		int insert = find_key(0);
		if (insert < KEY_STATE_MAX_LENGTH) {
			key_state_array[insert] = key;
		}
	}
}

void release_key(keycode key) {
	uint8_t index = find_key(key);
	if (index < KEY_STATE_MAX_LENGTH) {
		// shift it over and remove key
		key_state_array[index] = 0;
	}
}

// Pointer state and mode

struct pointer_state pointer_state;

static struct mode_state {
	// initial view state
	double x, y, w, h;
	swayc_t *ptr;
	// containers resized with tiling resize
	struct {
		double x, w;
		swayc_t *ptr;
		swayc_t *sib;
	} lr;
	struct {
		double y, h;
		swayc_t *ptr;
		swayc_t *sib;
	} tb;
} initial;

static struct {
	bool left;
	bool top;
} lock;

// initial set/unset

static void set_initial_view(swayc_t *view) {
	initial.ptr = view;
	initial.x = view->x;
	initial.y = view->y;
	initial.w = view->width;
	initial.h = view->height;
}

static void set_initial_sibling(void) {
	bool reset = true;
	if ((initial.lr.ptr = get_swayc_in_direction(initial.ptr, lock.left ? MOVE_RIGHT: MOVE_LEFT))) {
		initial.lr.sib = get_swayc_in_direction(initial.lr.ptr, lock.left ? MOVE_LEFT : MOVE_RIGHT);
		initial.lr.x = initial.lr.ptr->x;
		initial.lr.w = initial.lr.ptr->width;
		reset = false;
	}
	if ((initial.tb.ptr = get_swayc_in_direction(initial.ptr, lock.top ? MOVE_DOWN: MOVE_UP))) {
		initial.tb.sib = get_swayc_in_direction(initial.tb.ptr, lock.top ? MOVE_UP : MOVE_DOWN);
		initial.tb.y = initial.tb.ptr->y;
		initial.tb.h = initial.tb.ptr->height;
		reset = false;
	}
	// If nothing changes just undo the mode
	if (reset) {
		pointer_state.mode = 0;
	}
}

static void reset_initial_view(void) {
	initial.ptr->x = initial.x;
	initial.ptr->y = initial.y;
	initial.ptr->width = initial.w;
	initial.ptr->height = initial.h;
	arrange_windows(initial.ptr, -1, -1);
	pointer_state.mode = 0;
}

// Mode set left/right click

static void pointer_mode_set_left(void) {
	set_initial_view(pointer_state.left.view);
	if (initial.ptr->is_floating) {
		pointer_state.mode = M_DRAGGING | M_FLOATING;
	} else {
		pointer_state.mode = M_DRAGGING | M_TILING;
	}
}

static void pointer_mode_set_right(void) {
	set_initial_view(pointer_state.right.view);
	// Setup locking information
	int midway_x = initial.ptr->x + initial.ptr->width/2;
	int midway_y = initial.ptr->y + initial.ptr->height/2;

	lock.left = pointer_state.origin.x > midway_x;
	lock.top = pointer_state.origin.y > midway_y;

	if (initial.ptr->is_floating) {
		pointer_state.mode = M_RESIZING | M_FLOATING;
	} else {
		pointer_state.mode = M_RESIZING | M_TILING;
		set_initial_sibling();
	}
}

// Mode set/update/reset

void pointer_mode_set(uint32_t button, bool condition) {
	// switch on drag/resize mode
	switch (pointer_state.mode & (M_DRAGGING | M_RESIZING)) {
	case M_DRAGGING:
	// end drag mode when left click is unpressed
		if (!pointer_state.left.held) {
			pointer_state.mode = 0;
		}
		break;

	case M_RESIZING:
	// end resize mode when right click is unpressed
		if (!pointer_state.right.held) {
			pointer_state.mode = 0;
		}
		break;

	// No mode case
	default:
		// return if failed condition, or no view
		if (!condition || !pointer_state.view) {
			break;
		}

		// Set mode depending on current button press
		switch (button) {
		// Start dragging mode
		case M_LEFT_CLICK:
			// if button release dont do anything
			if (pointer_state.left.held) {
				pointer_mode_set_left();
			}
			break;

		// Start resize mode
		case M_RIGHT_CLICK:
			// if button release dont do anyhting
			if (pointer_state.right.held) {
				pointer_mode_set_right();
			}
			break;
		}
	}
}

void pointer_mode_update(void) {
	if (initial.ptr->type != C_VIEW) {
		pointer_state.mode = 0;
		return;
	}
	int dx = pointer_state.origin.x;
	int dy = pointer_state.origin.y;

	switch (pointer_state.mode) {
	case M_FLOATING | M_DRAGGING:
		// Update position
		dx -= pointer_state.left.x;
		dy -= pointer_state.left.y;
		if (initial.x + dx != initial.ptr->x) {
			initial.ptr->x = initial.x + dx;
		}
		if (initial.y + dy != initial.ptr->y) {
			initial.ptr->y = initial.y + dy;
		}
		update_geometry(initial.ptr);
		break;

	case M_FLOATING | M_RESIZING:
		dx -= pointer_state.right.x;
		dy -= pointer_state.right.y;
		initial.ptr = pointer_state.right.view;
		if (lock.left) {
			if (initial.w + dx > min_sane_w) {
				initial.ptr->width = initial.w + dx;
			}
		} else { //lock.right
			if (initial.w - dx > min_sane_w) {
				initial.ptr->width = initial.w - dx;
				initial.ptr->x = initial.x + dx;
			}
		}
		if (lock.top) {
			if (initial.h + dy > min_sane_h) {
				initial.ptr->height = initial.h + dy;
			}
		} else { //lock.bottom
			if (initial.h - dy > min_sane_h) {
				initial.ptr->height = initial.h - dy;
				initial.ptr->y = initial.y + dy;
			}
		}
		update_geometry(initial.ptr);
		break;

	case M_TILING | M_DRAGGING:
		// swap current view under pointer with dragged view
		if (pointer_state.view && pointer_state.view->type == C_VIEW
				&& pointer_state.view != initial.ptr) {
			// Swap them around
			swap_container(pointer_state.view, initial.ptr);
			update_geometry(pointer_state.view);
			update_geometry(initial.ptr);
			// Set focus back to initial view
			set_focused_container(initial.ptr);
		}
		break;

	case M_TILING | M_RESIZING:
		dx -= pointer_state.right.x;
		dy -= pointer_state.right.y;
		// resize if we can
		if (initial.lr.ptr) {
			if (lock.left) {
				// Check whether its fine to resize
				if (initial.w + dx > min_sane_w && initial.lr.w - dx > min_sane_w) {
					initial.lr.sib->width = initial.w + dx;
					initial.lr.ptr->width = initial.lr.w - dx;
				}
			} else { //lock.right
				if (initial.w - dx > min_sane_w && initial.lr.w + dx > min_sane_w) {
					initial.lr.sib->width = initial.w - dx;
					initial.lr.ptr->width = initial.lr.w + dx;
				}
			}
			arrange_windows(initial.lr.ptr->parent, -1, -1);
		}
		if (initial.tb.ptr) {
			if (lock.top) {
				if (initial.h + dy > min_sane_h && initial.tb.h - dy > min_sane_h) {
					initial.tb.sib->height = initial.h + dy;
					initial.tb.ptr->height = initial.tb.h - dy;
				}
			} else { //lock.bottom
				if (initial.h - dy > min_sane_h && initial.tb.h + dy > min_sane_h) {
					initial.tb.sib->height = initial.h - dy;
					initial.tb.ptr->height = initial.tb.h + dy;
				}
			}
			arrange_windows(initial.tb.ptr->parent, -1, -1);
		}
	default:
		return;
	}
}

void pointer_mode_reset(void) {
	switch (pointer_state.mode) {
	case M_FLOATING | M_DRAGGING:
	case M_FLOATING | M_RESIZING:
		reset_initial_view();
		break;

	case M_TILING | M_DRAGGING:
	case M_TILING | M_RESIZING:
	default:
		return;
	}
}

