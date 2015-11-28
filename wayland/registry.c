#include <wayland-client.h>
#include <stdlib.h>
#include <string.h>
#include "wayland-desktop-shell-client-protocol.h"
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
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct wl_output *output = wl_registry_bind(registry, name, &wl_output_interface, version);
		struct output_state *ostate = malloc(sizeof(struct output_state));
		ostate->output = output;
		wl_output_add_listener(output, &output_listener, ostate);
		list_add(reg->outputs, ostate);
	} else if (strcmp(interface, desktop_shell_interface.name) == 0) {
		reg->desktop_shell = wl_registry_bind(registry, name, &desktop_shell_interface, version);
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
