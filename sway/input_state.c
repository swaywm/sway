#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "log.h"
#include "config.h"

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

// Pointer state and mode

struct pointer_state pointer_state;

static struct mode_state {
	// initial view state
	double x, y, w, h;
	swayc_t *ptr;
	// Containers used for resizing horizontally
	struct {
		double w;
		swayc_t *ptr;
		struct {
			double w;
			swayc_t *ptr;
		} parent;
	} horiz;
	// Containers used for resizing vertically
	struct {
		double h;
		swayc_t *ptr;
		struct {
			double h;
			swayc_t *ptr;
		} parent;
	} vert;
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
	swayc_t *ws = swayc_active_workspace_for(initial.ptr);
	if ((initial.horiz.ptr = get_swayc_in_direction_under(initial.ptr,
			lock.left ? MOVE_RIGHT: MOVE_LEFT, ws))) {
		initial.horiz.w = initial.horiz.ptr->width;
		initial.horiz.parent.ptr = get_swayc_in_direction_under(initial.horiz.ptr,
			lock.left ? MOVE_LEFT : MOVE_RIGHT, ws);
		initial.horiz.parent.w = initial.horiz.parent.ptr->width;
		reset = false;
	}
	if ((initial.vert.ptr = get_swayc_in_direction_under(initial.ptr,
			lock.top ? MOVE_DOWN: MOVE_UP, ws))) {
		initial.vert.h = initial.vert.ptr->height;
		initial.vert.parent.ptr = get_swayc_in_direction_under(initial.vert.ptr,
			lock.top ? MOVE_UP : MOVE_DOWN, ws);
		initial.vert.parent.h = initial.vert.parent.ptr->height;
		reset = false;
	}
	// If nothing will change just undo the mode
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

static void reset_initial_sibling(void) {
	initial.horiz.ptr->width = initial.horiz.w;
	initial.horiz.parent.ptr->width = initial.horiz.parent.w;
	initial.vert.ptr->height = initial.vert.h;
	initial.vert.parent.ptr->height = initial.vert.parent.h;
	arrange_windows(initial.horiz.ptr->parent, -1, -1);
	arrange_windows(initial.vert.ptr->parent, -1, -1);
	pointer_state.mode = 0;
}

void pointer_position_set(struct wlc_origin *new_origin, bool force_focus) {
	pointer_state.delta.x = new_origin->x - pointer_state.origin.x;
	pointer_state.delta.y = new_origin->y - pointer_state.origin.y;
	pointer_state.origin.x = new_origin->x;
	pointer_state.origin.y = new_origin->y;

	// Update view under pointer
	swayc_t *prev_view = pointer_state.view;
	pointer_state.view = container_under_pointer();

	// If pointer is in a mode, update it
	if (pointer_state.mode) {
		pointer_mode_update();
	// Otherwise change focus if config is set
	} else if (force_focus || (prev_view != pointer_state.view && config->focus_follows_mouse)) {
		if (pointer_state.view && pointer_state.view->type == C_VIEW) {
			set_focused_container(pointer_state.view);
		}
	}

	wlc_pointer_set_origin(new_origin);
}

void center_pointer_on(swayc_t *view) {
	struct wlc_origin new_origin;
	new_origin.x = view->x + view->width/2;
	new_origin.y = view->y + view->height/2;
	pointer_position_set(&new_origin, true);
}

// Mode set left/right click

static void pointer_mode_set_left(void) {
	set_initial_view(pointer_state.left.view);
	if (initial.ptr->is_floating) {
		pointer_state.mode = M_DRAGGING | M_FLOATING;
	} else {
		pointer_state.mode = M_DRAGGING | M_TILING;
		// unset mode if we cant drag tile
		if (initial.ptr->parent->type == C_WORKSPACE &&
				initial.ptr->parent->children->length == 1) {
			pointer_state.mode = 0;
		}
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
		} else { // lock.right
			if (initial.w - dx > min_sane_w) {
				initial.ptr->width = initial.w - dx;
				initial.ptr->x = initial.x + dx;
			}
		}
		if (lock.top) {
			if (initial.h + dy > min_sane_h) {
				initial.ptr->height = initial.h + dy;
			}
		} else { // lock.bottom
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
				&& pointer_state.view != initial.ptr
				&& !pointer_state.view->is_floating) {
			// Swap them around
			swap_container(pointer_state.view, initial.ptr);
			swap_geometry(pointer_state.view, initial.ptr);
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
		if (initial.horiz.ptr) {
			if (lock.left) {
				// Check whether its fine to resize
				if (initial.w + dx > min_sane_w && initial.horiz.w - dx > min_sane_w) {
					initial.horiz.ptr->width = initial.horiz.w - dx;
					initial.horiz.parent.ptr->width = initial.horiz.parent.w + dx;
				}
			} else { // lock.right
				if (initial.w - dx > min_sane_w && initial.horiz.w + dx > min_sane_w) {
					initial.horiz.ptr->width = initial.horiz.w + dx;
					initial.horiz.parent.ptr->width = initial.horiz.parent.w - dx;
				}
			}
			arrange_windows(initial.horiz.ptr->parent, -1, -1);
		}
		if (initial.vert.ptr) {
			if (lock.top) {
				if (initial.h + dy > min_sane_h && initial.vert.h - dy > min_sane_h) {
					initial.vert.ptr->height = initial.vert.h - dy;
					initial.vert.parent.ptr->height = initial.vert.parent.h + dy;
				}
			} else { // lock.bottom
				if (initial.h - dy > min_sane_h && initial.vert.h + dy > min_sane_h) {
					initial.vert.ptr->height = initial.vert.h + dy;
					initial.vert.parent.ptr->height = initial.vert.parent.h - dy;
				}
			}
			arrange_windows(initial.vert.ptr->parent, -1, -1);
		}
	default:
		return;
	}
}

void pointer_mode_reset(void) {
	switch (pointer_state.mode) {
	case M_FLOATING | M_RESIZING:
	case M_FLOATING | M_DRAGGING:
		reset_initial_view();
		break;

	case M_TILING | M_RESIZING:
		(void) reset_initial_sibling;
		break;

	case M_TILING | M_DRAGGING:
	default:
		break;
	}
}
