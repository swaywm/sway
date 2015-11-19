#include <wayland-client.h>
#include <wayland-cursor.h>
#include "wayland-xdg-shell-client-protocol.h"
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include "client/client.h"
#include "client/buffer.h"
#include "list.h"
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

static void pointer_handle_enter(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface, wl_fixed_t sx_w, wl_fixed_t sy_w) {
	struct client_state *state = data;
	struct wl_cursor_image *image = state->cursor.cursor->images[0];
	wl_pointer_set_cursor(pointer, serial, state->cursor.surface, image->hotspot_x, image->hotspot_y);
}

static void pointer_handle_leave(void *data, struct wl_pointer *pointer,
		     uint32_t serial, struct wl_surface *surface) {
}

static void pointer_handle_motion(void *data, struct wl_pointer *pointer,
		      uint32_t time, wl_fixed_t sx_w, wl_fixed_t sy_w) {
}

static void pointer_handle_button(void *data, struct wl_pointer *pointer, uint32_t serial,
		      uint32_t time, uint32_t button, uint32_t state_w) {
}

static void pointer_handle_axis(void *data, struct wl_pointer *pointer,
		    uint32_t time, uint32_t axis, wl_fixed_t value) {
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = pointer_handle_enter,
	.leave = pointer_handle_leave,
	.motion = pointer_handle_motion,
	.button = pointer_handle_button,
	.axis = pointer_handle_axis
};

static void registry_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct client_state *state = data;

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, version);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->shm = wl_registry_bind(registry, name, &wl_shm_interface, version);
	} else if (strcmp(interface, wl_shell_interface.name) == 0) {
		state->shell = wl_registry_bind(registry, name, &wl_shell_interface, version);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		state->seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
		state->pointer = wl_seat_get_pointer(state->seat);
		wl_pointer_add_listener(state->pointer, &pointer_listener, state);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct wl_output *output = wl_registry_bind(registry, name, &wl_output_interface, version);
		struct output_state *ostate = malloc(sizeof(struct output_state));
		ostate->output = output;
		wl_output_add_listener(output, &output_listener, ostate);
		list_add(state->outputs, ostate);
	}
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
	// this space intentionally left blank
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove
};

void shell_surface_configure(void *data, struct wl_shell_surface *wl_shell_surface,
			  uint32_t edges, int32_t width, int32_t height) {
	struct client_state *state = data;
	state->width = width;
	state->height = height;
}

static const struct wl_shell_surface_listener surface_listener = {
	.configure = shell_surface_configure
};

struct client_state *client_setup(uint32_t width, uint32_t height) {
	struct client_state *state = malloc(sizeof(struct client_state));
	memset(state, 0, sizeof(struct client_state));
	state->outputs = create_list();
	state->width = width;
	state->height = height;

	state->display = wl_display_connect(NULL);
	if (!state->display) {
		sway_log(L_ERROR, "Error opening display");
		client_teardown(state);
		return NULL;
	}

	struct wl_registry *registry = wl_display_get_registry(state->display);
	wl_registry_add_listener(registry, &registry_listener, state);
	wl_display_dispatch(state->display);
	wl_display_roundtrip(state->display);
	wl_registry_destroy(registry);

	state->surface = wl_compositor_create_surface(state->compositor);
	state->shell_surface = wl_shell_get_shell_surface(state->shell, state->surface);
	wl_shell_surface_add_listener(state->shell_surface, &surface_listener, state);
	wl_shell_surface_set_toplevel(state->shell_surface);

	state->cursor.cursor_theme = wl_cursor_theme_load("default", 32, state->shm); // TODO: let you customize this
	state->cursor.cursor = wl_cursor_theme_get_cursor(state->cursor.cursor_theme, "left_ptr");
	state->cursor.surface = wl_compositor_create_surface(state->compositor);

	struct wl_cursor_image *image = state->cursor.cursor->images[0];
	struct wl_buffer *cursor_buf = wl_cursor_image_get_buffer(image);
	wl_surface_attach(state->cursor.surface, cursor_buf, 0, 0);
	wl_surface_damage(state->cursor.surface, 0, 0, image->width, image->height);
	wl_surface_commit(state->cursor.surface);

	return state;
}

static void frame_callback(void *data, struct wl_callback *callback, uint32_t time) {
	struct client_state *state = data;
	wl_callback_destroy(callback);
	state->frame_cb = NULL;
}

static const struct wl_callback_listener listener = {
	frame_callback
};

int client_prerender(struct client_state *state) {
	if (state->frame_cb) {
		return 0;
	}

	get_next_buffer(state);
	return 1;
}

int client_render(struct client_state *state) {
	state->frame_cb = wl_surface_frame(state->surface);
	wl_callback_add_listener(state->frame_cb, &listener, state);

	wl_surface_damage(state->surface, 0, 0, state->buffer->width, state->buffer->height);
	wl_surface_attach(state->surface, state->buffer->buffer, 0, 0);
	wl_surface_commit(state->surface);

	return 1;
}

void client_teardown(struct client_state *state) {
	if (state->pointer) {
		wl_pointer_destroy(state->pointer);
	}
	if (state->seat) {
		wl_seat_destroy(state->seat);
	}
	if (state->shell) {
		wl_shell_destroy(state->shell);
	}
	if (state->shm) {
		wl_shm_destroy(state->shm);
	}
	if (state->compositor) {
		wl_compositor_destroy(state->compositor);
	}
	if (state->display) {
		wl_display_disconnect(state->display);
	}
	if (state->outputs) {
		// TODO: Free the outputs themselves
		list_free(state->outputs);
	}
}
