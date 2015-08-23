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

struct pointer_state pointer_state;

// Pointer mode values
static struct mode_state {
	// Initial view state
	struct {
		double x, y, w, h;
		swayc_t *ptr;
	} view;
	// Initial pointer state
	struct {
		int x, y;
	} coor;
} initial;

static struct {
	enum { LEFT=1, RIGHT=0 } lr;
	enum { TOP=1, BOTTOM=0 } tb;
} lock;

// Floating set/unset

static void pointer_mode_set_floating(void) {
	initial.view.x = initial.view.ptr->x;
	initial.view.y = initial.view.ptr->y;
	initial.view.w = initial.view.ptr->width;
	initial.view.h = initial.view.ptr->height;
	// setup initial cooridinates
	initial.coor.x = pointer_state.origin.x;
	initial.coor.y = pointer_state.origin.y;
}

static void pointer_mode_reset_floating(void) {
	initial.view.ptr->x = initial.view.x;
	initial.view.ptr->y = initial.view.y;
	initial.view.ptr->width = initial.view.w;
	initial.view.ptr->height = initial.view.h;
	arrange_windows(initial.view.ptr, -1, -1);
	pointer_state.mode = 0;
}

// Mode set left/right click

static void pointer_mode_set_left(void) {
	swayc_t *view = pointer_state.view;
	initial.view.ptr = view;
	if (view->is_floating) {
		pointer_state.mode = M_DRAGGING | M_FLOATING;
		pointer_mode_set_floating();
	} else {
		pointer_state.mode = M_DRAGGING | M_TILING;
	}
}

static void pointer_mode_set_right(void) {
	swayc_t *view = pointer_state.view;
	initial.view.ptr = view;
	// Setup locking information
	int midway_x = view->x + view->width/2;
	int midway_y = view->y + view->height/2;

	lock.lr = pointer_state.origin.x > midway_x;
	lock.tb = pointer_state.origin.y > midway_y;

	if (view->is_floating) {
		pointer_state.mode = M_RESIZING | M_FLOATING;
		pointer_mode_set_floating();
	} else {
		pointer_state.mode = M_RESIZING | M_TILING;
	}
}

// Mode set/update/reset

void pointer_mode_set(uint32_t button, bool condition) {
	// switch on drag/resize mode
	switch (pointer_state.mode & (M_DRAGGING | M_RESIZING)) {
	case M_DRAGGING:
	// end drag mode when left click is unpressed
		if (!pointer_state.l_held) {
			pointer_state.mode = 0;
		}
		break;

	case M_RESIZING:
	// end resize mode when right click is unpressed
		if (!pointer_state.r_held) {
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
			if (pointer_state.l_held) {
				pointer_mode_set_left();
			}
			break;

		// Start resize mode
		case M_RIGHT_CLICK:
			// if button release dont do anyhting
			if (pointer_state.r_held) {
				pointer_mode_set_right();
			}
			break;

		case M_SCROLL_UP:
		case M_SCROLL_DOWN:
			//TODO add scrolling behavior here
			;
		}
	}
}

void pointer_mode_update(void) {
	swayc_t *view = initial.view.ptr;
	if (view->type != C_VIEW) {
		pointer_state.mode = 0;
		return;
	}
	int dx = pointer_state.origin.x - initial.coor.x;
	int dy = pointer_state.origin.y - initial.coor.y;
	bool changed = false;

	switch (pointer_state.mode) {
	case M_FLOATING | M_DRAGGING:
		// Update position
		if (initial.view.x + dx != view->x) {
			view->x = initial.view.x + dx;
			changed = true;
		}
		if (initial.view.y + dy != view->y) {
			view->y = initial.view.y + dy;
			changed = true;
		}
		break;

	case M_FLOATING | M_RESIZING:
		if (lock.lr) {
			if (initial.view.w + dx > min_sane_w) {
				if (initial.view.w + dx != view->width) {
					view->width = initial.view.w + dx;
					changed = true;
				}
			}
		} else { //lock.right
			if (initial.view.w - dx > min_sane_w) {
				if (initial.view.w - dx != view->width) {
					view->width = initial.view.w - dx;
					view->x = initial.view.x + dx;
					changed = true;
				}
			}
		}
		if (lock.tb) {
			if (initial.view.h + dy > min_sane_h) {
				if (initial.view.y - dy != view->height) {
					view->height = initial.view.h + dy;
					changed = true;
				}
			}
		} else { //lock.bottom
			if (initial.view.h - dy > min_sane_h) {
				if (initial.view.h - dy != view->height) {
					view->height = initial.view.h - dy;
					view->y = initial.view.y + dy;
					changed = true;
				}
			}
		}
		break;

	case M_TILING | M_DRAGGING:
		// swap current view under pointer with dragged view
		if (pointer_state.view && pointer_state.view != initial.view.ptr) {
			// Swap them around
			swap_container(pointer_state.view, initial.view.ptr);
			update_geometry(pointer_state.view);
			update_geometry(initial.view.ptr);
			// Set focus back to initial view
			set_focused_container(initial.view.ptr);
		}
		break;

	case M_TILING | M_RESIZING:
		
		

	default:
		return;
	}
	if (changed) {
		update_geometry(view);
	}
}

void pointer_mode_reset(void) {
	switch (pointer_state.mode) {
	case M_FLOATING | M_DRAGGING:
	case M_FLOATING | M_RESIZING:
		pointer_mode_reset_floating();
		break;

	case M_TILING | M_DRAGGING:
	case M_TILING | M_RESIZING:
	default:
		return;
	}
}


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

