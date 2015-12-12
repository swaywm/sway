#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include "wayland-desktop-shell-client-protocol.h"
#include "wayland-swaylock-client-protocol.h"
#include "client/registry.h"
#include "stringop.h"
#include "log.h"

static void display_handle_mode(void *data, struct wl_output *wl_output,
		     uint32_t flags, int32_t width, int32_t height, int32_t refresh) {
	struct output_state *state = data;
	if (flags & WL_OUTPUT_MODE_CURRENT) {
		state->flags = flags;
		state->width = width;
		state->height = height;
	}
}

static void display_handle_geometry(void *data, struct wl_output *wl_output,
			 int32_t x, int32_t y, int32_t physical_width, int32_t physical_height,
			 int32_t subpixel, const char *make, const char *model, int32_t transform) {
	// this space intentionally left blank
}

static void display_handle_done(void *data, struct wl_output *wl_output) {
	// this space intentionally left blank
}

static void display_handle_scale(void *data, struct wl_output *wl_output, int32_t factor) {
	// this space intentionally left blank
}

static const struct wl_output_listener output_listener = {
	.mode = display_handle_mode,
	.geometry = display_handle_geometry,
	.done = display_handle_done,
	.scale = display_handle_scale
};

const char *XKB_MASK_NAMES[MASK_LAST] = {
	XKB_MOD_NAME_SHIFT,
	XKB_MOD_NAME_CAPS,
	XKB_MOD_NAME_CTRL,
	XKB_MOD_NAME_ALT,
	"Mod2",
	"Mod3",
	XKB_MOD_NAME_LOGO,
	"Mod5",
};

const enum mod_bit XKB_MODS[MASK_LAST] = {
	MOD_SHIFT,
	MOD_CAPS,
	MOD_CTRL,
	MOD_ALT,
	MOD_MOD2,
	MOD_MOD3,
	MOD_LOGO,
	MOD_MOD5
};

static void keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
		uint32_t format, int fd, uint32_t size) {
	// Keyboard errors are abort-worthy because you wouldn't be able to unlock your screen otherwise.

	struct registry *registry = data;
	if (!data) {
		close(fd);
		return;
	}

	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		sway_abort("Unknown keymap format %d, aborting", format);
	}

	char *map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	if (map_str == MAP_FAILED) {
		close(fd);
		sway_abort("Unable to initialized shared keyboard memory, aborting");
	}

	struct xkb_keymap *keymap = xkb_keymap_new_from_string(registry->input->xkb.context,
			map_str, XKB_KEYMAP_FORMAT_TEXT_V1, 0);
	munmap(map_str, size);
	close(fd);

	if (!keymap) {
		sway_abort("Failed to compile keymap, aborting");
	}

	struct xkb_state *state = xkb_state_new(keymap);
	if (!state) {
		xkb_keymap_unref(keymap);
		sway_abort("Failed to create xkb state, aborting");
	}

	xkb_keymap_unref(registry->input->xkb.keymap);
	xkb_state_unref(registry->input->xkb.state);
	registry->input->xkb.keymap = keymap;
	registry->input->xkb.state = state;

	int i;
	for (i = 0; i < MASK_LAST; ++i) {
		registry->input->xkb.masks[i] = 1 << xkb_keymap_mod_get_index(registry->input->xkb.keymap, XKB_MASK_NAMES[i]);
	}
}

static void keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
		uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
	// this space intentionally left blank
}

static void keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
		uint32_t serial, struct wl_surface *surface) {
	// this space intentionally left blank
}

static void keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
		uint32_t serial, uint32_t time, uint32_t key, uint32_t state_w) {
	struct registry *registry = data;
	enum wl_keyboard_key_state state = state_w;

	if (!registry->input->xkb.state) {
		return;
	}

	xkb_keysym_t sym = xkb_state_key_get_one_sym(registry->input->xkb.state, key + 8);
	registry->input->sym = (state == WL_KEYBOARD_KEY_STATE_PRESSED ? sym : XKB_KEY_NoSymbol);
	registry->input->code = (state == WL_KEYBOARD_KEY_STATE_PRESSED ? key + 8 : 0);
	uint32_t codepoint = xkb_state_key_get_utf32(registry->input->xkb.state, registry->input->code);
	if (registry->input->notify) {
		registry->input->notify(state, sym, key, codepoint);
	}
}

static void keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
		uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
		uint32_t mods_locked, uint32_t group) {
	struct registry *registry = data;

	if (!registry->input->xkb.keymap) {
		return;
	}

	xkb_state_update_mask(registry->input->xkb.state, mods_depressed, mods_latched, mods_locked, 0, 0, group);
	xkb_mod_mask_t mask = xkb_state_serialize_mods(registry->input->xkb.state,
			XKB_STATE_MODS_DEPRESSED | XKB_STATE_MODS_LATCHED);

	registry->input->modifiers = 0;
	for (uint32_t i = 0; i < MASK_LAST; ++i) {
		if (mask & registry->input->xkb.masks[i]) {
			registry->input->modifiers |= XKB_MODS[i];
		}
	}
}

static void keyboard_handle_repeat_info(void *data, struct wl_keyboard *keyboard,
		int32_t rate, int32_t delay) {
	// this space intentionally left blank
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_handle_keymap,
	.enter = keyboard_handle_enter,
	.leave = keyboard_handle_leave,
	.key = keyboard_handle_key,
	.modifiers = keyboard_handle_modifiers,
	.repeat_info = keyboard_handle_repeat_info
};

static void registry_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct registry *reg = data;

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		reg->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, version);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		reg->shm = wl_registry_bind(registry, name, &wl_shm_interface, version);
	} else if (strcmp(interface, wl_shell_interface.name) == 0) {
		reg->shell = wl_registry_bind(registry, name, &wl_shell_interface, version);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		reg->seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
		reg->pointer = wl_seat_get_pointer(reg->seat);
		reg->keyboard = wl_seat_get_keyboard(reg->seat);
		if (reg->keyboard) {
			wl_keyboard_add_listener(reg->keyboard, &keyboard_listener, reg);
		}
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct wl_output *output = wl_registry_bind(registry, name, &wl_output_interface, version);
		struct output_state *ostate = malloc(sizeof(struct output_state));
		ostate->output = output;
		wl_output_add_listener(output, &output_listener, ostate);
		list_add(reg->outputs, ostate);
	} else if (strcmp(interface, desktop_shell_interface.name) == 0) {
		reg->desktop_shell = wl_registry_bind(registry, name, &desktop_shell_interface, version);
	} else if (strcmp(interface, lock_interface.name) == 0) {
		reg->swaylock = wl_registry_bind(registry, name, &lock_interface, version);
	}
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
	// this space intentionally left blank
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove
};

struct registry *registry_poll(void) {
	struct registry *registry = malloc(sizeof(struct registry));
	memset(registry, 0, sizeof(struct registry));
	registry->outputs = create_list();
	registry->input = calloc(sizeof(struct input), 1);
	registry->input->xkb.context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);

	registry->display = wl_display_connect(NULL);
	if (!registry->display) {
		sway_log(L_ERROR, "Error opening display");
		registry_teardown(registry);
		return NULL;
	}

	struct wl_registry *reg = wl_display_get_registry(registry->display);
	wl_registry_add_listener(reg, &registry_listener, registry);
	wl_display_dispatch(registry->display);
	wl_display_roundtrip(registry->display);
	wl_registry_destroy(reg);

	return registry;
}

void registry_teardown(struct registry *registry) {
	if (registry->pointer) {
		wl_pointer_destroy(registry->pointer);
	}
	if (registry->seat) {
		wl_seat_destroy(registry->seat);
	}
	if (registry->shell) {
		wl_shell_destroy(registry->shell);
	}
	if (registry->shm) {
		wl_shm_destroy(registry->shm);
	}
	if (registry->compositor) {
		wl_compositor_destroy(registry->compositor);
	}
	if (registry->display) {
		wl_display_disconnect(registry->display);
	}
	if (registry->outputs) {
		free_flat_list(registry->outputs);
	}
}
