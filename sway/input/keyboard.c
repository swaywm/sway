#include <assert.h>
#include <limits.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_idle.h>
#include "sway/input/seat.h"
#include "sway/input/keyboard.h"
#include "sway/input/input-manager.h"
#include "sway/commands.h"
#include "log.h"

static bool keysym_is_modifier(xkb_keysym_t keysym) {
	switch (keysym) {
	case XKB_KEY_Shift_L: case XKB_KEY_Shift_R:
	case XKB_KEY_Control_L: case XKB_KEY_Control_R:
	case XKB_KEY_Caps_Lock:
	case XKB_KEY_Shift_Lock:
	case XKB_KEY_Meta_L: case XKB_KEY_Meta_R:
	case XKB_KEY_Alt_L: case XKB_KEY_Alt_R:
	case XKB_KEY_Super_L: case XKB_KEY_Super_R:
	case XKB_KEY_Hyper_L: case XKB_KEY_Hyper_R:
		return true;
	default:
		return false;
	}
}

static size_t pressed_keysyms_length(xkb_keysym_t *pressed_keysyms) {
	size_t n = 0;
	for (size_t i = 0; i < SWAY_KEYBOARD_PRESSED_KEYSYMS_CAP; ++i) {
		if (pressed_keysyms[i] != XKB_KEY_NoSymbol) {
			++n;
		}
	}
	return n;
}

static ssize_t pressed_keysyms_index(xkb_keysym_t *pressed_keysyms,
		xkb_keysym_t keysym) {
	for (size_t i = 0; i < SWAY_KEYBOARD_PRESSED_KEYSYMS_CAP; ++i) {
		if (pressed_keysyms[i] == keysym) {
			return i;
		}
	}
	return -1;
}

static void pressed_keysyms_add(xkb_keysym_t *pressed_keysyms,
		xkb_keysym_t keysym) {
	ssize_t i = pressed_keysyms_index(pressed_keysyms, keysym);
	if (i < 0) {
		i = pressed_keysyms_index(pressed_keysyms, XKB_KEY_NoSymbol);
		if (i >= 0) {
			pressed_keysyms[i] = keysym;
		}
	}
}

static void pressed_keysyms_remove(xkb_keysym_t *pressed_keysyms,
		xkb_keysym_t keysym) {
	ssize_t i = pressed_keysyms_index(pressed_keysyms, keysym);
	if (i >= 0) {
		pressed_keysyms[i] = XKB_KEY_NoSymbol;
	}
}

static void pressed_keysyms_update(xkb_keysym_t *pressed_keysyms,
		const xkb_keysym_t *keysyms, size_t keysyms_len,
		enum wlr_key_state state) {
	for (size_t i = 0; i < keysyms_len; ++i) {
		if (keysym_is_modifier(keysyms[i])) {
			continue;
		}
		if (state == WLR_KEY_PRESSED) {
			pressed_keysyms_add(pressed_keysyms, keysyms[i]);
		} else { // WLR_KEY_RELEASED
			pressed_keysyms_remove(pressed_keysyms, keysyms[i]);
		}
	}
}

static bool binding_matches_key_state(struct sway_binding *binding,
		enum wlr_key_state key_state) {
	if (key_state == WLR_KEY_PRESSED && !binding->release) {
		return true;
	}
	if (key_state == WLR_KEY_RELEASED && binding->release) {
		return true;
	}

	return false;
}

static void keyboard_execute_command(struct sway_keyboard *keyboard,
		struct sway_binding *binding) {
	wlr_log(L_DEBUG, "running command for binding: %s",
		binding->command);
	config_clear_handler_context(config);
	config->handler_context.seat = keyboard->seat_device->sway_seat;
	struct cmd_results *results = execute_command(binding->command, NULL);
	if (results->status != CMD_SUCCESS) {
		wlr_log(L_DEBUG, "could not run command for binding: %s (%s)",
			binding->command, results->error);
	}
	free_cmd_results(results);
}

/**
 * Execute a built-in, hardcoded compositor binding. These are triggered from a
 * single keysym.
 *
 * Returns true if the keysym was handled by a binding and false if the event
 * should be propagated to clients.
 */
static bool keyboard_execute_compositor_binding(struct sway_keyboard *keyboard,
		xkb_keysym_t *pressed_keysyms, uint32_t modifiers, size_t keysyms_len) {
	for (size_t i = 0; i < keysyms_len; ++i) {
		xkb_keysym_t keysym = pressed_keysyms[i];
		if (keysym >= XKB_KEY_XF86Switch_VT_1 &&
				keysym <= XKB_KEY_XF86Switch_VT_12) {
			if (wlr_backend_is_multi(server.backend)) {
				struct wlr_session *session =
					wlr_multi_get_session(server.backend);
				if (session) {
					unsigned vt = keysym - XKB_KEY_XF86Switch_VT_1 + 1;
					wlr_session_change_vt(session, vt);
				}
			}
			return true;
		}
	}

	return false;
}

/**
 * Execute keyboard bindings bound with `bindysm` for the given keyboard state.
 *
 * Returns true if the keysym was handled by a binding and false if the event
 * should be propagated to clients.
 */
static bool keyboard_execute_bindsym(struct sway_keyboard *keyboard,
		xkb_keysym_t *pressed_keysyms, uint32_t modifiers,
		enum wlr_key_state key_state) {
	// configured bindings
	int n = pressed_keysyms_length(pressed_keysyms);
	list_t *keysym_bindings = config->current_mode->keysym_bindings;
	for (int i = 0; i < keysym_bindings->length; ++i) {
		struct sway_binding *binding = keysym_bindings->items[i];
		if (!binding_matches_key_state(binding, key_state) ||
				modifiers ^ binding->modifiers ||
				n != binding->keys->length) {
			continue;
		}

		bool match = true;
		for (int j = 0; j < binding->keys->length; ++j) {
			match =
				pressed_keysyms_index(pressed_keysyms,
					*(int*)binding->keys->items[j]) >= 0;

			if (!match) {
				break;
			}
		}

		if (match) {
			keyboard_execute_command(keyboard, binding);
			return true;
		}
	}

	return false;
}

static bool binding_matches_keycodes(struct wlr_keyboard *keyboard,
		struct sway_binding *binding, struct wlr_event_keyboard_key *event) {
	assert(binding->bindcode);

	uint32_t keycode = event->keycode + 8;

	if (!binding_matches_key_state(binding, event->state)) {
		return false;
	}

	uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard);
	if (modifiers ^ binding->modifiers) {
		return false;
	}

	// on release, the released key must be in the binding
	if (event->state == WLR_KEY_RELEASED) {
		bool found = false;
		for (int i = 0; i < binding->keys->length; ++i) {
			uint32_t binding_keycode = *(uint32_t*)binding->keys->items[i] + 8;
			if (binding_keycode == keycode) {
				found = true;
				break;
			}
		}
		if (!found) {
			return false;
		}
	}

	// every keycode in the binding must be present in the pressed keys on the
	// keyboard
	for (int i = 0; i < binding->keys->length; ++i) {
		uint32_t binding_keycode = *(uint32_t*)binding->keys->items[i] + 8;
		if (event->state == WLR_KEY_RELEASED && keycode == binding_keycode) {
			continue;
		}

		bool found = false;
		for (size_t j = 0; j < keyboard->num_keycodes; ++j) {
			xkb_keycode_t keycode = keyboard->keycodes[j] + 8;
			if (keycode == binding_keycode) {
				found = true;
				break;
			}
		}

		if (!found) {
			return false;
		}
	}

	// every keycode pressed on the keyboard must be present within the binding
	// keys (unless it is a modifier)
	for (size_t i = 0; i < keyboard->num_keycodes; ++i) {
		xkb_keycode_t keycode = keyboard->keycodes[i] + 8;
		bool found = false;
		for (int j = 0; j < binding->keys->length; ++j) {
			uint32_t binding_keycode = *(uint32_t*)binding->keys->items[j] + 8;
			if (binding_keycode == keycode) {
				found = true;
				break;
			}
		}

		if (!found) {
			if (!binding->modifiers) {
				return false;
			}

			// check if it is a modifier, which we know matched from the check
			// above
			const xkb_keysym_t *keysyms;
			int num_keysyms =
				xkb_state_key_get_syms(keyboard->xkb_state,
					keycode, &keysyms);
			if (num_keysyms != 1 || !keysym_is_modifier(keysyms[0])) {
				return false;
			}
		}
	}

	return true;
}

/**
 * Execute keyboard bindings bound with `bindcode` for the given keyboard state.
 *
 * Returns true if the keysym was handled by a binding and false if the event
 * should be propagated to clients.
 */
static bool keyboard_execute_bindcode(struct sway_keyboard *keyboard,
		struct wlr_event_keyboard_key *event) {
	struct wlr_keyboard *wlr_keyboard =
		keyboard->seat_device->input_device->wlr_device->keyboard;
	list_t *keycode_bindings = config->current_mode->keycode_bindings;
	for (int i = 0; i < keycode_bindings->length; ++i) {
		struct sway_binding *binding = keycode_bindings->items[i];
		if (binding_matches_keycodes(wlr_keyboard, binding, event)) {
			keyboard_execute_command(keyboard, binding);
			return true;
		}
	}

	return false;
}

/**
 * Get keysyms and modifiers from the keyboard as xkb sees them.
 *
 * This uses the xkb keysyms translation based on pressed modifiers and clears
 * the consumed modifiers from the list of modifiers passed to keybind
 * detection.
 *
 * On US layout, pressing Alt+Shift+2 will trigger Alt+@.
 */
static size_t keyboard_keysyms_translated(struct sway_keyboard *keyboard,
		xkb_keycode_t keycode, const xkb_keysym_t **keysyms,
		uint32_t *modifiers) {
	struct wlr_input_device *device =
		keyboard->seat_device->input_device->wlr_device;
	*modifiers = wlr_keyboard_get_modifiers(device->keyboard);
	xkb_mod_mask_t consumed = xkb_state_key_get_consumed_mods2(
		device->keyboard->xkb_state, keycode, XKB_CONSUMED_MODE_XKB);
	*modifiers = *modifiers & ~consumed;

	return xkb_state_key_get_syms(device->keyboard->xkb_state,
		keycode, keysyms);
}

/**
 * Get keysyms and modifiers from the keyboard as if modifiers didn't change
 * keysyms.
 *
 * This avoids the xkb keysym translation based on modifiers considered pressed
 * in the state.
 *
 * This will trigger keybinds such as Alt+Shift+2.
 */
static size_t keyboard_keysyms_raw(struct sway_keyboard *keyboard,
		xkb_keycode_t keycode, const xkb_keysym_t **keysyms,
		uint32_t *modifiers) {
	struct wlr_input_device *device =
		keyboard->seat_device->input_device->wlr_device;
	*modifiers = wlr_keyboard_get_modifiers(device->keyboard);

	xkb_layout_index_t layout_index = xkb_state_key_get_layout(
		device->keyboard->xkb_state, keycode);
	return xkb_keymap_key_get_syms_by_level(device->keyboard->keymap,
		keycode, layout_index, 0, keysyms);
}

static void handle_keyboard_key(struct wl_listener *listener, void *data) {
	struct sway_keyboard *keyboard =
		wl_container_of(listener, keyboard, keyboard_key);
	struct wlr_seat *wlr_seat = keyboard->seat_device->sway_seat->wlr_seat;
	struct wlr_input_device *wlr_device =
		keyboard->seat_device->input_device->wlr_device;
	wlr_idle_notify_activity(
			keyboard->seat_device->sway_seat->input->server->idle, wlr_seat);
	struct wlr_event_keyboard_key *event = data;

	xkb_keycode_t keycode = event->keycode + 8;
	bool handled = false;

	// handle keycodes
	handled = keyboard_execute_bindcode(keyboard, event);

	// handle translated keysyms
	if (!handled && event->state == WLR_KEY_RELEASED) {
		handled = keyboard_execute_bindsym(keyboard,
			keyboard->pressed_keysyms_translated,
			keyboard->modifiers_translated,
			event->state);
	}
	const xkb_keysym_t *translated_keysyms;
	size_t translated_keysyms_len =
		keyboard_keysyms_translated(keyboard, keycode, &translated_keysyms,
			&keyboard->modifiers_translated);
	pressed_keysyms_update(keyboard->pressed_keysyms_translated,
		translated_keysyms, translated_keysyms_len, event->state);
	if (!handled && event->state == WLR_KEY_PRESSED) {
		handled = keyboard_execute_bindsym(keyboard,
			keyboard->pressed_keysyms_translated,
			keyboard->modifiers_translated,
			event->state);
	}

	// Handle raw keysyms
	if (!handled && event->state == WLR_KEY_RELEASED) {
		handled = keyboard_execute_bindsym(keyboard,
			keyboard->pressed_keysyms_raw, keyboard->modifiers_raw,
			event->state);
	}
	const xkb_keysym_t *raw_keysyms;
	size_t raw_keysyms_len =
		keyboard_keysyms_raw(keyboard, keycode, &raw_keysyms, &keyboard->modifiers_raw);
	pressed_keysyms_update(keyboard->pressed_keysyms_raw, raw_keysyms,
		raw_keysyms_len, event->state);
	if (!handled && event->state == WLR_KEY_PRESSED) {
		handled = keyboard_execute_bindsym(keyboard,
			keyboard->pressed_keysyms_raw, keyboard->modifiers_raw,
			event->state);
	}

	// Compositor bindings
	if (!handled && event->state == WLR_KEY_PRESSED) {
		handled =
			keyboard_execute_compositor_binding(keyboard,
				keyboard->pressed_keysyms_translated,
				keyboard->modifiers_translated,
				translated_keysyms_len);
	}
	if (!handled && event->state == WLR_KEY_PRESSED) {
		handled =
			keyboard_execute_compositor_binding(keyboard,
				keyboard->pressed_keysyms_raw, keyboard->modifiers_raw,
				raw_keysyms_len);
	}

	if (!handled || event->state == WLR_KEY_RELEASED) {
		wlr_seat_set_keyboard(wlr_seat, wlr_device);
		wlr_seat_keyboard_notify_key(wlr_seat, event->time_msec,
				event->keycode, event->state);
	}
}

static void handle_keyboard_modifiers(struct wl_listener *listener,
		void *data) {
	struct sway_keyboard *keyboard =
		wl_container_of(listener, keyboard, keyboard_modifiers);
	struct wlr_seat *wlr_seat = keyboard->seat_device->sway_seat->wlr_seat;
	struct wlr_input_device *wlr_device =
		keyboard->seat_device->input_device->wlr_device;
	wlr_seat_set_keyboard(wlr_seat, wlr_device);
	wlr_seat_keyboard_notify_modifiers(wlr_seat, &wlr_device->keyboard->modifiers);
}

struct sway_keyboard *sway_keyboard_create(struct sway_seat *seat,
		struct sway_seat_device *device) {
	struct sway_keyboard *keyboard =
		calloc(1, sizeof(struct sway_keyboard));
	if (!sway_assert(keyboard, "could not allocate sway keyboard")) {
		return NULL;
	}

	keyboard->seat_device = device;
	device->keyboard = keyboard;

	wl_list_init(&keyboard->keyboard_key.link);
	wl_list_init(&keyboard->keyboard_modifiers.link);

	return keyboard;
}

void sway_keyboard_configure(struct sway_keyboard *keyboard) {
	struct xkb_rule_names rules;
	memset(&rules, 0, sizeof(rules));
	struct input_config *input_config =
		input_device_get_config(keyboard->seat_device->input_device);
	struct wlr_input_device *wlr_device =
		keyboard->seat_device->input_device->wlr_device;

	if (input_config && input_config->xkb_layout) {
		rules.layout = input_config->xkb_layout;
	} else {
		rules.layout = getenv("XKB_DEFAULT_LAYOUT");
	}
	if (input_config && input_config->xkb_model) {
		rules.model = input_config->xkb_model;
	} else {
		rules.model = getenv("XKB_DEFAULT_MODEL");
	}

	if (input_config && input_config->xkb_options) {
		rules.options = input_config->xkb_options;
	} else {
		rules.options = getenv("XKB_DEFAULT_OPTIONS");
	}

	if (input_config && input_config->xkb_rules) {
		rules.rules = input_config->xkb_rules;
	} else {
		rules.rules = getenv("XKB_DEFAULT_RULES");
	}

	if (input_config && input_config->xkb_variant) {
		rules.variant = input_config->xkb_variant;
	} else {
		rules.variant = getenv("XKB_DEFAULT_VARIANT");
	}

	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!sway_assert(context, "cannot create XKB context")) {
		return;
	}

	struct xkb_keymap *keymap =
		xkb_keymap_new_from_names(context, &rules, XKB_KEYMAP_COMPILE_NO_FLAGS);

	if (!keymap) {
		wlr_log(L_DEBUG, "cannot configure keyboard: keymap does not exist");
		xkb_context_unref(context);
		return;
	}

	xkb_keymap_unref(keyboard->keymap);
	keyboard->keymap = keymap;
	wlr_keyboard_set_keymap(wlr_device->keyboard, keyboard->keymap);

	if (input_config && input_config->repeat_delay != INT_MIN
			&& input_config->repeat_rate != INT_MIN) {
		wlr_keyboard_set_repeat_info(wlr_device->keyboard,
				input_config->repeat_rate, input_config->repeat_delay);
	} else {
		wlr_keyboard_set_repeat_info(wlr_device->keyboard, 25, 600);
	}
	xkb_context_unref(context);
	struct wlr_seat *seat = keyboard->seat_device->sway_seat->wlr_seat;
	wlr_seat_set_keyboard(seat, wlr_device);

	wl_list_remove(&keyboard->keyboard_key.link);
	wl_signal_add(&wlr_device->keyboard->events.key, &keyboard->keyboard_key);
	keyboard->keyboard_key.notify = handle_keyboard_key;

	wl_list_remove(&keyboard->keyboard_modifiers.link);
	wl_signal_add( &wlr_device->keyboard->events.modifiers,
		&keyboard->keyboard_modifiers);
	keyboard->keyboard_modifiers.notify = handle_keyboard_modifiers;
}

void sway_keyboard_destroy(struct sway_keyboard *keyboard) {
	if (!keyboard) {
		return;
	}
	wl_list_remove(&keyboard->keyboard_key.link);
	wl_list_remove(&keyboard->keyboard_modifiers.link);
	free(keyboard);
}
