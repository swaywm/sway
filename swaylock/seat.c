#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include "swaylock/swaylock.h"
#include "swaylock/seat.h"

static void keyboard_keymap(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t format, int32_t fd, uint32_t size) {
	struct swaylock_state *state = data;
	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		wlr_log(WLR_ERROR, "Unknown keymap format %d, aborting", format);
		exit(1);
	}
	char *map_shm = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (map_shm == MAP_FAILED) {
		close(fd);
		wlr_log(WLR_ERROR, "Unable to initialize keymap shm, aborting");
		exit(1);
	}
	struct xkb_keymap *keymap = xkb_keymap_new_from_string(
			state->xkb.context, map_shm, XKB_KEYMAP_FORMAT_TEXT_V1, 0);
	munmap(map_shm, size);
	close(fd);
	assert(keymap);
	struct xkb_state *xkb_state = xkb_state_new(keymap);
	assert(xkb_state);
	xkb_keymap_unref(state->xkb.keymap);
	xkb_state_unref(state->xkb.state);
	state->xkb.keymap = keymap;
	state->xkb.state = xkb_state;
}

static void keyboard_enter(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
	// Who cares
}

static void keyboard_leave(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface) {
	// Who cares
}

static void keyboard_key(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t time, uint32_t key, uint32_t _key_state) {
	struct swaylock_state *state = data;
	enum wl_keyboard_key_state key_state = _key_state;
	xkb_keysym_t sym = xkb_state_key_get_one_sym(state->xkb.state, key + 8);
	uint32_t keycode = key_state == WL_KEYBOARD_KEY_STATE_PRESSED ?
		key + 8 : 0;
	uint32_t codepoint = xkb_state_key_get_utf32(state->xkb.state, keycode);
	if (key_state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		swaylock_handle_key(state, sym, codepoint);
	}
}

static void keyboard_modifiers(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
		uint32_t mods_locked, uint32_t group) {
	struct swaylock_state *state = data;
	xkb_state_update_mask(state->xkb.state,
		mods_depressed, mods_latched, mods_locked, 0, 0, group);
	state->xkb.caps_lock = xkb_state_mod_name_is_active(state->xkb.state,
		XKB_MOD_NAME_CAPS, XKB_STATE_MODS_LOCKED);
	state->xkb.control = xkb_state_mod_name_is_active(state->xkb.state,
		XKB_MOD_NAME_CTRL,
		XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED);

}

static void keyboard_repeat_info(void *data, struct wl_keyboard *wl_keyboard,
		int32_t rate, int32_t delay) {
	// TODO
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_keymap,
	.enter = keyboard_enter,
	.leave = keyboard_leave,
	.key = keyboard_key,
	.modifiers = keyboard_modifiers,
	.repeat_info = keyboard_repeat_info,
};

static void wl_pointer_enter(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface,
		wl_fixed_t surface_x, wl_fixed_t surface_y) {
	wl_pointer_set_cursor(wl_pointer, serial, NULL, 0, 0);
}

static void wl_pointer_leave(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface) {
	// Who cares
}

static void wl_pointer_motion(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	// Who cares
}

static void wl_pointer_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
	// Who cares
}

static void wl_pointer_axis(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis, wl_fixed_t value) {
	// Who cares
}

static void wl_pointer_frame(void *data, struct wl_pointer *wl_pointer) {
	// Who cares
}

static void wl_pointer_axis_source(void *data, struct wl_pointer *wl_pointer,
		uint32_t axis_source) {
	// Who cares
}

static void wl_pointer_axis_stop(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis) {
	// Who cares
}

static void wl_pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer,
		uint32_t axis, int32_t discrete) {
	// Who cares
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = wl_pointer_enter,
	.leave = wl_pointer_leave,
	.motion = wl_pointer_motion,
	.button = wl_pointer_button,
	.axis = wl_pointer_axis,
	.frame = wl_pointer_frame,
	.axis_source = wl_pointer_axis_source,
	.axis_stop = wl_pointer_axis_stop,
	.axis_discrete = wl_pointer_axis_discrete,
};

static void seat_handle_capabilities(void *data, struct wl_seat *wl_seat,
		enum wl_seat_capability caps) {
	struct swaylock_state *state = data;
	if ((caps & WL_SEAT_CAPABILITY_POINTER)) {
		struct wl_pointer *pointer = wl_seat_get_pointer(wl_seat);
		wl_pointer_add_listener(pointer, &pointer_listener, NULL);
	}
	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD)) {
		struct wl_keyboard *keyboard = wl_seat_get_keyboard(wl_seat);
		wl_keyboard_add_listener(keyboard, &keyboard_listener, state);
	}
}

static void seat_handle_name(void *data, struct wl_seat *wl_seat,
		const char *name) {
	// Who cares
}

const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
	.name = seat_handle_name,
};
