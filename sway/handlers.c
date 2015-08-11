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

bool handle_output_created(wlc_handle output) {
	add_output(output);
	return true;
}

void handle_output_destroyed(wlc_handle output) {
	destroy_output(output);
}

void handle_output_resolution_change(wlc_handle output, const struct wlc_size *from, const struct wlc_size *to) {
	sway_log(L_DEBUG, "Output %d resolution changed to %d x %d", output, to->w, to->h);
	swayc_t *c = get_swayc_for_handle(output, &root_container);
	if (!c) return;
	c->width = to->w;
	c->height = to->h;
	arrange_windows(&root_container, -1, -1);
}

bool handle_view_created(wlc_handle view) {
	add_view(view);
	return true;
}

void handle_view_destroyed(wlc_handle view) {
	sway_log(L_DEBUG, "Destroying window %d", view);
	destroy_view(get_swayc_for_handle(view, &root_container));
	return true;
}

void handle_view_focus(wlc_handle view, bool focus) {
	return;
}

void handle_view_geometry_request(wlc_handle view, const struct wlc_geometry* geometry) {
	// deny that shit
}

bool handle_key(wlc_handle view, uint32_t time, const struct wlc_modifiers
		*modifiers, uint32_t key, uint32_t sym, enum wlc_key_state state) {
	// TODO: handle keybindings with more than 1 non-modifier key involved
	// Note: reminder to check conflicts with mod+q+a versus mod+q
	
	bool cmd_success = true;
	struct sway_mode *mode = config->current_mode;

	// Lowercase if necessary
	sym = tolower(sym);

	int i;
	for (i = 0; i < mode->bindings->length; ++i) {
		struct sway_binding *binding = mode->bindings->items[i];

		if ((modifiers->mods & binding->modifiers) == binding->modifiers) {
			bool match = true;
			int j;
			for (j = 0; j < binding->keys->length; ++j) {
				xkb_keysym_t *k = binding->keys->items[j];
				if (sym != *k) {
					match = false;
					break;
				}
			}

			if (match) {
				// TODO: --released
				if (state == WLC_KEY_STATE_PRESSED) {
					cmd_success = !handle_command(config, binding->command);
				} else {
					cmd_success = true;
				}
			}
		}
	}
	return cmd_success;
}

bool pointer_test(swayc_t *view, void *_origin) {
	const struct wlc_origin *origin = _origin;
	if (view->type == C_VIEW && origin->x >= view->x && origin->y >= view->y
			&& origin->x < view->x + view->width && origin->y < view->y + view->height
			&& view->visible) {
		return true;
	}
	return false;
}

struct wlc_origin mouse_origin;

bool handle_pointer_motion(wlc_handle view, uint32_t time, const struct wlc_origin *origin) {
	mouse_origin = *origin;
	if (!config->focus_follows_mouse) {
		return true;
	}
	swayc_t *c = find_container(&root_container, pointer_test, (void *)origin);
	swayc_t *focused = get_focused_container(&root_container);
	if (c && c != focused) {
		sway_log(L_DEBUG, "Switching focus to %p", c);
		unfocus_all(&root_container);
		focus_view(c);
	}
	return true;
}

bool handle_pointer_button(wlc_handle view, uint32_t time, const struct wlc_modifiers *modifiers,
		uint32_t button, enum wlc_button_state state) {
	if (state == WLC_BUTTON_STATE_PRESSED) {
		swayc_t *c = find_container(&root_container, pointer_test, &mouse_origin);
		swayc_t *focused = get_focused_container(&root_container);
		if (c && c != focused) {
			sway_log(L_DEBUG, "Switching focus to %p", c);
			unfocus_all(&root_container);
			focus_view(c);
			return false;
		}
		return true;
	}
	return true;
}
