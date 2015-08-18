#include <xkbcommon/xkbcommon.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wlc/wlc.h>
#include <ctype.h>
#include "layout.h"
#include "log.h"
#include "config.h"
#include "commands.h"
#include "handlers.h"
#include "stringop.h"
#include "workspace.h"
#include "container.h"

uint32_t keys_pressed[32];

static struct wlc_origin mouse_origin;
//Keyboard input is being overrided by window (dmenu)
static bool override_redirect = false;

static bool m1_held = false;
static bool m2_held = false;

static bool pointer_test(swayc_t *view, void *_origin) {
	const struct wlc_origin *origin = _origin;
	//Determine the output that the view is under
	swayc_t *parent = view;
	while (parent->type != C_OUTPUT) {
		parent = parent->parent;
	}
	if (view->type == C_VIEW && origin->x >= view->x && origin->y >= view->y
			&& origin->x < view->x + view->width && origin->y < view->y + view->height
			&& view->visible && parent == root_container.focused) {
		return true;
	}
	return false;
}

swayc_t *focus_pointer(void) {
	swayc_t *focused = get_focused_container(&root_container);
	if (!(wlc_view_get_state(focused->handle) & WLC_BIT_FULLSCREEN)) {
		swayc_t *pointer = find_container(&root_container, pointer_test, &mouse_origin);
		if (pointer && focused != pointer) {
			unfocus_all(&root_container);
			focus_view(pointer);
		} else if (!focused) {
			focus_view(active_workspace);
		}
		focused = pointer;
	}
	return focused;
}

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
	if (!c) return;
	if (focus) {
		unfocus_all(&root_container);
		focus_view(c);
	}
}

static bool handle_view_created(wlc_handle handle) {
	swayc_t *focused = get_focused_container(&root_container);
	uint32_t type = wlc_view_get_type(handle);
	// If override_redirect/unmanaged/popup/modal/splach
	if (type) {
		sway_log(L_DEBUG,"Unmanaged window of type %x left alone", type);
		wlc_view_set_state(handle, WLC_BIT_ACTIVATED, true);
		if (type & WLC_BIT_UNMANAGED) {
			return true;
		}
		// For things like Dmenu
		if (type & WLC_BIT_OVERRIDE_REDIRECT) {
			override_redirect = true;
			wlc_view_focus(handle);
		}

		// Float popups
		if (type & WLC_BIT_POPUP) {
			swayc_t *view = new_floating_view(handle);
			wlc_view_set_state(handle, WLC_BIT_MAXIMIZED, false);
			focus_view(view);
			arrange_windows(active_workspace, -1, -1);
		}
	} else {
		swayc_t *view = new_view(focused, handle);
		//Set maximize flag for windows.
		//TODO: floating windows have this unset
		wlc_view_set_state(handle, WLC_BIT_MAXIMIZED, true);
		unfocus_all(&root_container);
		focus_view(view);
		arrange_windows(view->parent, -1, -1);
	}
	if (wlc_view_get_state(focused->handle) & WLC_BIT_FULLSCREEN) {
		unfocus_all(&root_container);
		focus_view(focused);
		arrange_windows(focused, -1, -1);
	}
	return true;
}

static void handle_view_destroyed(wlc_handle handle) {
	sway_log(L_DEBUG, "Destroying window %u", (unsigned int)handle);

	// Properly handle unmanaged views
	uint32_t type = wlc_view_get_type(handle);
	if (type) {
		wlc_view_set_state(handle, WLC_BIT_ACTIVATED, true);
		sway_log(L_DEBUG,"Unmanaged window of type %x was destroyed", type);
		if (type & WLC_BIT_UNMANAGED) {
			// We need to call focus_view() on focus_pointer because unmanaged windows
			// do not alter the focus structure of the container tree. This makes focus_pointer()
			// think that it doesn't need to do anything, so we manually focus the result.
			focus_view(focus_pointer());
			return;
		}

		if (type & WLC_BIT_OVERRIDE_REDIRECT) {
			override_redirect = false;
			focus_view(focus_pointer());
			return;
		}

		// WLC_BIT_POPUP doesn't need to be dealt with since it's
		// treated as a floating view.
	}

	swayc_t *view = get_swayc_for_handle(handle, &root_container);
	swayc_t *parent;
	swayc_t *focused = get_focused_container(&root_container);

	if (view) {
		parent = destroy_view(view);
		arrange_windows(parent, -1, -1);
	}
	if (!focused || focused == view) {
		focus_pointer();
	}
}

static void handle_view_focus(wlc_handle view, bool focus) {
	return;
}

static void handle_view_geometry_request(wlc_handle handle, const struct wlc_geometry* geometry) {
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
	switch(state) {
	case WLC_BIT_FULLSCREEN:
		{
			//I3 just lets it become fullscreen
			wlc_view_set_state(view,state,toggle);
			swayc_t *c = get_swayc_for_handle(view, &root_container);
			sway_log(L_DEBUG, "setting view %ld %s, fullscreen %d",view,c->name,toggle);
			if (c) {
				arrange_windows(c->parent, -1, -1);
				//Set it as focused window for that workspace if its going
				//fullscreen
				if (toggle) {
					swayc_t *ws = c;
					while (ws->type != C_WORKSPACE) {
						ws = ws->parent;
					}
					//Set ws focus to c
					focus_view_for(ws, c);
				}
			}
			break;
		}
	case WLC_BIT_MAXIMIZED:
	case WLC_BIT_RESIZING:
	case WLC_BIT_MOVING:
	case WLC_BIT_ACTIVATED:
		break;
	}
	return;
}


static bool handle_key(wlc_handle view, uint32_t time, const struct wlc_modifiers
		*modifiers, uint32_t key, uint32_t sym, enum wlc_key_state state) {
	enum { QSIZE = 32 };
	if (override_redirect) {
		return false;
	}
	static uint8_t  head = 0;
	bool cmd_success = false;

	struct sway_mode *mode = config->current_mode;
	// Lowercase if necessary
	sym = tolower(sym);

	//Find key, if it has been pressed
	int mid = 0;
	while (mid < head && keys_pressed[mid] != sym) {
		++mid;
	}
	if (state == WLC_KEY_STATE_PRESSED && mid == head && head + 1 < QSIZE) {
		keys_pressed[head++] = sym;
	} else if (state == WLC_KEY_STATE_RELEASED && mid < head) {
		memmove(keys_pressed + mid, keys_pressed + mid + 1, sizeof*keys_pressed * (--head - mid));
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
				for (k = 0; k < head; ++k) {
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
				//Remove matched keys from keys_pressed
				int j;
				for (j = 0; j < binding->keys->length; ++j) {
					uint8_t k;
					for (k = 0; k < head; ++k) {
						memmove(keys_pressed + k, keys_pressed + k + 1, sizeof*keys_pressed * (--head - k));
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
	int i = 0;
	// Do checks to determine if proper keys are being held
	swayc_t *view = active_workspace->focused;
	if (m1_held) {
		if (view->is_floating) {
			while (keys_pressed[i++]) {
				if (keys_pressed[i] == config->floating_mod) {
					int dx = mouse_origin.x - prev_pos.x;
					int dy = mouse_origin.y - prev_pos.y;
					sway_log(L_DEBUG, "Moving from px: %d to cx: %d and from py: %d to cy: %d", prev_pos.x, mouse_origin.x, prev_pos.y, mouse_origin.y);
					sway_log(L_DEBUG, "Moving: dx: %d, dy: %d", dx, dy);

					view->x += dx;
					view->y += dy;
					changed_floating = true;
					break;
				}
			}
		}
	} else if (m2_held) {
		if (view->is_floating) {
			while (keys_pressed[i++]) {
				if (keys_pressed[i] == config->floating_mod) {
					int dx = mouse_origin.x - prev_pos.x;
					int dy = mouse_origin.y - prev_pos.y;
					sway_log(L_DEBUG, "Moving from px: %d to cx: %d and from py: %d to cy: %d", prev_pos.x, mouse_origin.x, prev_pos.y, mouse_origin.y);
					sway_log(L_INFO, "Moving: dx: %d, dy: %d", dx, dy);

					// Move and resize the view based on the dx/dy and mouse position
					int midway_x = view->x + view->width/2;
					int midway_y = view->y + view->height/2;

					if (dx < 0) {
						changed_floating = true;
						if (mouse_origin.x > midway_x) {
							sway_log(L_INFO, "Downsizing view to the left");
							view->width += dx;
						} else {
							sway_log(L_INFO, "Upsizing view to the left");
							view->x += dx;
							view->width -= dx;
						}
					} else if (dx > 0){
						changed_floating = true;
						if (mouse_origin.x > midway_x) {
							sway_log(L_INFO, "Upsizing to the right");
							view->width += dx;
						} else {
							sway_log(L_INFO, "Downsizing to the right");
							view->x += dx;
							view->width -= dx;
						}
					}

					if (dy < 0) {
						changed_floating = true;
						if (mouse_origin.y > midway_y) {
							sway_log(L_INFO, "Downsizing view to the top");
							view->height += dy;
						} else {
							sway_log(L_INFO, "Upsizing the view to the top");
							view->y += dy;
							view->height -= dy;
						}
					} else if (dy > 0) {
						changed_floating = true;
						if (mouse_origin.y > midway_y) {
							sway_log(L_INFO, "Upsizing to the bottom");
							view->height += dy;
						} else {
							sway_log(L_INFO, "Downsizing to the bottom");
							view->y += dy;
							view->height -= dy;
						}
					}
					break;
				}
			}
		}
	}
	if (config->focus_follows_mouse && prev_handle != handle) {
		focus_pointer();
	}
	prev_handle = handle;
	prev_pos = mouse_origin;
	if (changed_floating) {
		arrange_windows(view, -1, -1);
		return true;
	}
	return false;
}

static bool handle_pointer_button(wlc_handle view, uint32_t time, const struct wlc_modifiers *modifiers,
		uint32_t button, enum wlc_button_state state) {
	swayc_t *focused = get_focused_container(&root_container);
	if (state == WLC_BUTTON_STATE_PRESSED) {
		sway_log(L_DEBUG, "Mouse button %u pressed", button);
		if (button == 272) {
			m1_held = true;
		}
		if (button == 273) {
			m2_held = true;
		}
		swayc_t *pointer = focus_pointer();
		return (pointer && pointer != focused);
	} else {
		sway_log(L_DEBUG, "Mouse button %u released", button);
		if (button == 272) {
			m1_held = false;
		}
		if (button == 273) {
			m2_held = false;
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
