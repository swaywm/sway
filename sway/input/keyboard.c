#include "sway/input/seat.h"
#include "sway/input/keyboard.h"
#include "log.h"

static void handle_keyboard_key(struct wl_listener *listener, void *data) {
	struct sway_keyboard *keyboard =
		wl_container_of(listener, keyboard, keyboard_key);
	struct wlr_event_keyboard_key *event = data;
	wlr_seat_set_keyboard(keyboard->seat->seat, keyboard->device);
	wlr_seat_keyboard_notify_key(keyboard->seat->seat, event->time_msec,
		event->keycode, event->state);
}

static void handle_keyboard_modifiers(struct wl_listener *listener,
		void *data) {
	struct sway_keyboard *keyboard =
		wl_container_of(listener, keyboard, keyboard_modifiers);
	wlr_seat_set_keyboard(keyboard->seat->seat, keyboard->device);
	wlr_seat_keyboard_notify_modifiers(keyboard->seat->seat);
}

struct sway_keyboard *sway_keyboard_create(struct sway_seat *seat,
		struct wlr_input_device *device) {
	struct sway_keyboard *keyboard =
		calloc(1, sizeof(struct sway_keyboard));
	if (!sway_assert(keyboard, "could not allocate sway keyboard")) {
		return NULL;
	}

	keyboard->device = device;
	keyboard->seat = seat;

	// TODO keyboard config
	struct xkb_rule_names rules;
	memset(&rules, 0, sizeof(rules));
	rules.rules = getenv("XKB_DEFAULT_RULES");
	rules.model = getenv("XKB_DEFAULT_MODEL");
	rules.layout = getenv("XKB_DEFAULT_LAYOUT");
	rules.variant = getenv("XKB_DEFAULT_VARIANT");
	rules.options = getenv("XKB_DEFAULT_OPTIONS");
	struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!sway_assert(context, "cannot create XKB context")) {
		return NULL;
	}

	wlr_keyboard_set_keymap(device->keyboard, xkb_map_new_from_names(context,
		&rules, XKB_KEYMAP_COMPILE_NO_FLAGS));
	xkb_context_unref(context);

	wl_signal_add(&device->keyboard->events.key, &keyboard->keyboard_key);
	keyboard->keyboard_key.notify = handle_keyboard_key;

	wl_signal_add(&device->keyboard->events.modifiers, &keyboard->keyboard_modifiers);
	keyboard->keyboard_modifiers.notify = handle_keyboard_modifiers;

	return keyboard;
}
