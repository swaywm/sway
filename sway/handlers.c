#include <xkbcommon/xkbcommon.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include <ctype.h>

#include "handlers.h"
#include "log.h"
#include "layout.h"
#include "config.h"
#include "commands.h"
#include "stringop.h"
#include "workspace.h"
#include "container.h"
#include "focus.h"

uint32_t keys_pressed[32];
int keys_pressed_length = 0;


static struct wlc_origin mouse_origin;

static bool m1_held = false;
static bool dragging = false;
static bool m2_held = false;
static bool resizing = false;
static bool lock_left, lock_right, lock_top, lock_bottom = false;

static bool pointer_test(swayc_t *view, void *_origin) {
	const struct wlc_origin *origin = _origin;
	// Determine the output that the view is under
	swayc_t *parent = view;
	while (parent->type != C_OUTPUT) {
		parent = parent->parent;
	}
	if (origin->x >= view->x && origin->y >= view->y
		&& origin->x < view->x + view->width && origin->y < view->y + view->height
		&& view->visible && parent == root_container.focused) {
		return true;
	}
	return false;
}

swayc_t *container_under_pointer(void) {
	// root.output->workspace
	if (!root_container.focused || !root_container.focused->focused) {
		return NULL;
	}
	swayc_t *lookup = root_container.focused->focused;
	// Case of empty workspace
	if (lookup->children == 0) {
		return NULL;
	}
	while (lookup->type != C_VIEW) {
		int i;
		int len;
		// if tabbed/stacked go directly to focused container, otherwise search
		// children
		if (lookup->layout == L_TABBED || lookup->layout == L_STACKED) {
			lookup = lookup->focused;
			continue;
		}
		// if workspace, search floating
		if (lookup->type == C_WORKSPACE) {
			i = len = lookup->floating->length;
			bool got_floating = false;
			while (--i > -1) {
				if (pointer_test(lookup->floating->items[i], &mouse_origin)) {
					lookup = lookup->floating->items[i];
					got_floating = true;
					break;
				}
			}
			if (got_floating) {
				continue;
			}
		}
		// search children
		len = lookup->children->length;
		for (i = 0; i < len; ++i) {
			if (pointer_test(lookup->children->items[i], &mouse_origin)) {
				lookup = lookup->children->items[i];
				break;
			}
		}
		// when border and titles are done, this could happen
		if (i == len) {
			break;
		}
	}
	return lookup;
}

static struct wlc_geometry saved_floating;

static void start_floating(swayc_t *view) {
	if (view->is_floating) {
		saved_floating.origin.x = view->x;
		saved_floating.origin.y = view->y;
		saved_floating.size.w = view->width;
		saved_floating.size.h = view->height;
	}
}

static void reset_floating(swayc_t *view) {
	if (view->is_floating) {
		view->x = saved_floating.origin.x;
		view->y = saved_floating.origin.y;
		view->width = saved_floating.size.w;
		view->height = saved_floating.size.h;
		arrange_windows(view->parent, -1, -1);
	}
	dragging = resizing = false;
	lock_left = lock_right = lock_top = lock_bottom = false;
}

/* Handles */

static bool handle_output_created(wlc_handle output) {
	swayc_t *op = new_output(output);

	//Switch to workspace if we need to
	if (active_workspace == NULL) {
		swayc_t *ws = op->children->items[0];
		workspace_switch(ws);
	}
	return true;
}

static void handle_output_destroyed(wlc_handle output) {
	int i;
	list_t *list = root_container.children;
	for (i = 0; i < list->length; ++i) {
		if (((swayc_t *)list->items[i])->handle == output) {
			break;
		}
	}
	if (i < list->length) {
		destroy_output(list->items[i]);
	}
	if (list->length == 0) {
		active_workspace = NULL;
	} else {
		//switch to other outputs active workspace
		workspace_switch(((swayc_t *)root_container.children->items[0])->focused);
	}
}

static void handle_output_resolution_change(wlc_handle output, const struct wlc_size *from, const struct wlc_size *to) {
	sway_log(L_DEBUG, "Output %u resolution changed to %d x %d", (unsigned int)output, to->w, to->h);
	swayc_t *c = get_swayc_for_handle(output, &root_container);
	if (!c) return;
	c->width = to->w;
	c->height = to->h;
	arrange_windows(&root_container, -1, -1);
}

static void handle_output_focused(wlc_handle output, bool focus) {
	swayc_t *c = get_swayc_for_handle(output, &root_container);
	// if for some reason this output doesnt exist, create it.
	if (!c) {
		handle_output_created(output);
	}
	if (focus) {
		set_focused_container(c);
	}
}

static bool handle_view_created(wlc_handle handle) {
	// if view is child of another view, the use that as focused container
	wlc_handle parent = wlc_view_get_parent(handle);
	swayc_t *focused = NULL;
	swayc_t *newview = NULL;

	// Get parent container, to add view in
	if (parent) {
		focused = get_swayc_for_handle(parent, &root_container);
	}
	if (!focused || focused->type == C_OUTPUT) {
		focused = get_focused_container(&root_container);
	}
	sway_log(L_DEBUG, "creating view %ld with type %x, state %x, with parent %ld",
		handle, wlc_view_get_type(handle), wlc_view_get_state(handle), parent);

	// TODO properly figure out how each window should be handled.
	switch (wlc_view_get_type(handle)) {
	// regular view created regularly
	case 0:
		newview = new_view(focused, handle);
		wlc_view_set_state(handle, WLC_BIT_MAXIMIZED, true);
		break;

	// Dmenu keeps viewfocus, but others with this flag dont, for now simulate
	// dmenu
	case WLC_BIT_OVERRIDE_REDIRECT:
//		locked_view_focus = true;
		wlc_view_focus(handle);
		wlc_view_set_state(handle, WLC_BIT_ACTIVATED, true);
		wlc_view_bring_to_front(handle);
		break;

	// Firefox popups have this flag set.
	case WLC_BIT_OVERRIDE_REDIRECT|WLC_BIT_UNMANAGED:
		wlc_view_bring_to_front(handle);
		locked_container_focus = true;
		break;

	// Modals, get focus, popups do not
	case WLC_BIT_MODAL:
		wlc_view_focus(handle);
		wlc_view_bring_to_front(handle);
		newview = new_floating_view(handle);
	case WLC_BIT_POPUP:
		wlc_view_bring_to_front(handle);
		break;
	}

	if (newview) {
		set_focused_container(newview);
		swayc_t *output = newview->parent;
		while (output && output->type != C_OUTPUT) {
			output = output->parent;
		}
		arrange_windows(output, -1, -1);
	}
	return true;
}

static void handle_view_destroyed(wlc_handle handle) {
	sway_log(L_DEBUG, "Destroying window %lu", handle);
	swayc_t *view = get_swayc_for_handle(handle, &root_container);

	switch (wlc_view_get_type(handle)) {
	// regular view created regularly
	case 0:
	case WLC_BIT_MODAL:
	case WLC_BIT_POPUP:
		if (view) {
			swayc_t *parent = destroy_view(view);
			arrange_windows(parent, -1, -1);
		}
		break;
	// DMENU has this flag, and takes view_focus, but other things with this
	// flag dont
	case WLC_BIT_OVERRIDE_REDIRECT:
//		locked_view_focus = false;
		break;
	case WLC_BIT_OVERRIDE_REDIRECT|WLC_BIT_UNMANAGED:
		locked_container_focus = false;
		break;
	}
	set_focused_container(get_focused_view(&root_container));
}

static void handle_view_focus(wlc_handle view, bool focus) {
	return;
}

static void handle_view_geometry_request(wlc_handle handle, const struct wlc_geometry *geometry) {
	sway_log(L_DEBUG, "geometry request %d x %d : %d x %d",
			geometry->origin.x, geometry->origin.y, geometry->size.w, geometry->size.h);
	// If the view is floating, then apply the geometry.
	// Otherwise save the desired width/height for the view.
	// This will not do anything for the time being as WLC improperly sends geometry requests
	swayc_t *view = get_swayc_for_handle(handle, &root_container);
	if (view) {
		view->desired_width = geometry->size.w;
		view->desired_height = geometry->size.h;

		if (view->is_floating) {
			view->width = view->desired_width;
			view->height = view->desired_height;
			view->x = geometry->origin.x;
			view->y = geometry->origin.y;
			arrange_windows(view->parent, -1, -1);
		}
	}
}

static void handle_view_state_request(wlc_handle view, enum wlc_view_state_bit state, bool toggle) {
	swayc_t *c = NULL;
	switch (state) {
	case WLC_BIT_FULLSCREEN:
		// i3 just lets it become fullscreen
		wlc_view_set_state(view, state, toggle);
		c = get_swayc_for_handle(view, &root_container);
		sway_log(L_DEBUG, "setting view %ld %s, fullscreen %d", view, c->name, toggle);
		if (c) {
			arrange_windows(c->parent, -1, -1);
			// Set it as focused window for that workspace if its going fullscreen
			if (toggle) {
				swayc_t *ws = c;
				while (ws->type != C_WORKSPACE) {
					ws = ws->parent;
				}
				// Set ws focus to c
				set_focused_container_for(ws, c);
			}
		}
		break;
	case WLC_BIT_MAXIMIZED:
	case WLC_BIT_RESIZING:
	case WLC_BIT_MOVING:
	case WLC_BIT_ACTIVATED:
		break;
	}
	return;
}


static bool handle_key(wlc_handle view, uint32_t time, const struct wlc_modifiers *modifiers,
		uint32_t key, uint32_t sym, enum wlc_key_state state) {
	enum { QSIZE = 32 };
	if (locked_view_focus && state == WLC_KEY_STATE_PRESSED) {
		return false;
	}
	bool cmd_success = false;

	if ((modifiers->mods & config->floating_mod) && (dragging || resizing)) {
		reset_floating(get_focused_view(&root_container));
	}

	struct sway_mode *mode = config->current_mode;
	// Lowercase if necessary
	sym = tolower(sym);

	// Find key, if it has been pressed
	int mid = 0;
	while (mid < keys_pressed_length && keys_pressed[mid] != sym) {
		++mid;
	}
	//Add or remove key depending on state
	if (state == WLC_KEY_STATE_PRESSED && mid == keys_pressed_length && keys_pressed_length + 1 < QSIZE) {
		keys_pressed[keys_pressed_length++] = sym;
	} else if (state == WLC_KEY_STATE_RELEASED && mid < keys_pressed_length) {
		memmove(keys_pressed + mid, keys_pressed + mid + 1, sizeof*keys_pressed * (--keys_pressed_length - mid));
		keys_pressed[keys_pressed_length] = 0;
	}
	// TODO: reminder to check conflicts with mod+q+a versus mod+q
	int i;
	for (i = 0; i < mode->bindings->length; ++i) {
		struct sway_binding *binding = mode->bindings->items[i];

		if ((modifiers->mods & binding->modifiers) == binding->modifiers) {
			bool match;
			int j;
			for (j = 0; j < binding->keys->length; ++j) {
				match = false;
				xkb_keysym_t *key = binding->keys->items[j];
				uint8_t k;
				for (k = 0; k < keys_pressed_length; ++k) {
					if (keys_pressed[k] == *key) {
						match = true;
						break;
					}
				}
				if (match == false) {
					break;
				}
			}

			if (match) {
				// Remove matched keys from keys_pressed
				int j;
				for (j = 0; j < binding->keys->length; ++j) {
					uint8_t k;
					for (k = 0; k < keys_pressed_length; ++k) {
						memmove(keys_pressed + k, keys_pressed + k + 1, sizeof*keys_pressed * (--keys_pressed_length - k));
						keys_pressed[keys_pressed_length] = 0;
						break;
					}
				}
				if (state == WLC_KEY_STATE_PRESSED) {
					cmd_success = handle_command(config, binding->command);
				} else if (state == WLC_KEY_STATE_RELEASED) {
					// TODO: --released
				}
			}
		}
	}
	return cmd_success;
}

static bool handle_pointer_motion(wlc_handle handle, uint32_t time, const struct wlc_origin *origin) {
	static struct wlc_origin prev_pos;
	static wlc_handle prev_handle = 0;
	mouse_origin = *origin;
	bool changed_floating = false;
	if (!active_workspace) {
		return false;
	}
	// Do checks to determine if proper keys are being held
	swayc_t *view = get_focused_view(active_workspace);
	uint32_t edge = 0;
	if (dragging && view) {
		if (view->is_floating) {
			int dx = mouse_origin.x - prev_pos.x;
			int dy = mouse_origin.y - prev_pos.y;
			view->x += dx;
			view->y += dy;
			changed_floating = true;
		}
	} else if (resizing && view) {
		if (view->is_floating) {
			int dx = mouse_origin.x - prev_pos.x;
			int dy = mouse_origin.y - prev_pos.y;
			int min_sane_w = 100;
			int min_sane_h = 60;

			// Move and resize the view based on the dx/dy and mouse position
			int midway_x = view->x + view->width/2;
			int midway_y = view->y + view->height/2;
			if (dx < 0) {
				if (!lock_right) {
					if (view->width > min_sane_w) {
						changed_floating = true;
						view->width += dx;
						edge += WLC_RESIZE_EDGE_RIGHT;
					}
				} else if (mouse_origin.x < midway_x && !lock_left) {
					changed_floating = true;
					view->x += dx;
					view->width -= dx;
					edge += WLC_RESIZE_EDGE_LEFT;
				}
			} else if (dx > 0){
				if (mouse_origin.x > midway_x && !lock_right) {
					changed_floating = true;
					view->width += dx;
					edge += WLC_RESIZE_EDGE_RIGHT;
				} else if (!lock_left) {
					if (view->width > min_sane_w) {
						changed_floating = true;
						view->x += dx;
						view->width -= dx;
						edge += WLC_RESIZE_EDGE_LEFT;
					}
				}
			}

			if (dy < 0) {
				if (!lock_bottom) {
					if (view->height > min_sane_h) {
						changed_floating = true;
						view->height += dy;
						edge += WLC_RESIZE_EDGE_BOTTOM;
					}
				} else if (mouse_origin.y < midway_y && !lock_top) {
					changed_floating = true;
					view->y += dy;
					view->height -= dy;
					edge += WLC_RESIZE_EDGE_TOP;
				}
			} else if (dy > 0) {
				if (mouse_origin.y > midway_y && !lock_bottom) {
					changed_floating = true;
					view->height += dy;
					edge += WLC_RESIZE_EDGE_BOTTOM;
				} else if (!lock_top) {
					if (view->height > min_sane_h) {
						changed_floating = true;
						view->y += dy;
						view->height -= dy;
						edge += WLC_RESIZE_EDGE_TOP;
					}
				}
			}
		}
	}
	if (config->focus_follows_mouse && prev_handle != handle) {
		//Dont change focus if fullscreen
		swayc_t *focused = get_focused_view(view);
		if (!(focused->type == C_VIEW && wlc_view_get_state(focused->handle) & WLC_BIT_FULLSCREEN) && !(m1_held || m2_held)) {
			set_focused_container(container_under_pointer());
		}
	}
	prev_handle = handle;
	prev_pos = mouse_origin;
	if (changed_floating) {
		struct wlc_geometry geometry = {
			.origin = {
				.x = view->x,
				.y = view->y
			},
			.size = {
				.w = view->width,
				.h = view->height
			}
		};
		wlc_view_set_geometry(view->handle, edge, &geometry);
		return true;
	}
	return false;
}

static bool handle_pointer_button(wlc_handle view, uint32_t time, const struct wlc_modifiers *modifiers,
		uint32_t button, enum wlc_button_state state, const struct wlc_origin *origin) {
	swayc_t *focused = get_focused_container(&root_container);
	//dont change focus if fullscreen
	if (focused->type == C_VIEW && wlc_view_get_state(focused->handle) & WLC_BIT_FULLSCREEN) {
		return false;
	}
	if (state == WLC_BUTTON_STATE_PRESSED) {
		sway_log(L_DEBUG, "Mouse button %u pressed", button);
		if (button == 272) {
			m1_held = true;
		}
		if (button == 273) {
			m2_held = true;
		}
		swayc_t *pointer = container_under_pointer();
		set_focused_container(pointer);
		if (pointer->is_floating) {
			int i;
			for (i = 0; i < pointer->parent->floating->length; i++) {
				if (pointer->parent->floating->items[i] == pointer) {
					list_del(pointer->parent->floating, i);
					list_add(pointer->parent->floating, pointer);
					break;
				}
			}
			arrange_windows(pointer->parent, -1, -1);
			if (modifiers->mods & config->floating_mod) {
				dragging = m1_held;
				resizing = m2_held;
				int midway_x = pointer->x + pointer->width/2;
				int midway_y = pointer->y + pointer->height/2;
				lock_bottom = origin->y < midway_y;
				lock_top = !lock_bottom;
				lock_right = origin->x < midway_x;
				lock_left = !lock_right;
				start_floating(pointer);
			}
			//Dont want pointer sent to window while dragging or resizing
			return (dragging || resizing);
		}
		return (pointer && pointer != focused);
	} else {
		sway_log(L_DEBUG, "Mouse button %u released", button);
		if (button == 272) {
			m1_held = false;
			dragging = false;
		}
		if (button == 273) {
			m2_held = false;
			resizing = false;
			lock_top = lock_bottom = lock_left = lock_right = false;
		}
	}
	return false;
}

static void handle_wlc_ready(void) {
	sway_log(L_DEBUG, "Compositor is ready, executing cmds in queue");

	int i;
	for (i = 0; i < config->cmd_queue->length; ++i) {
		handle_command(config, config->cmd_queue->items[i]);
	}
	free_flat_list(config->cmd_queue);
	config->active = true;
}


struct wlc_interface interface = {
	.output = {
		.created = handle_output_created,
		.destroyed = handle_output_destroyed,
		.resolution = handle_output_resolution_change,
		.focus = handle_output_focused
	},
	.view = {
		.created = handle_view_created,
		.destroyed = handle_view_destroyed,
		.focus = handle_view_focus,
		.request = {
			.geometry = handle_view_geometry_request,
			.state = handle_view_state_request
		}
	},
	.keyboard = {
		.key = handle_key
	},
	.pointer = {
		.motion = handle_pointer_motion,
		.button = handle_pointer_button
	},
	.compositor = {
		.ready = handle_wlc_ready
	}
};
