#include <wayland-client.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/mman.h>
#include "client.h"
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

static int create_pool_file(size_t size) {
    static const char template[] = "/swaybg-XXXXXX";
    const char *path = getenv("XDG_RUNTIME_DIR");
	if (!path) {
        return -1;
    }

    int ts = (path[strlen(path) - 1] == '/');

    char *name = malloc(
		strlen(template) +
		strlen(path) +
		(ts ? 1 : 0) + 1);
	sprintf(name, "%s%s%s", path, ts ? "" : "/", template);

    int fd = mkstemp(name);
    free(name);

    if (fd < 0) {
        return -1;
    }

    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static void buffer_release(void *data, struct wl_buffer *buffer) {
	struct client_state *state = data;
	state->busy = false;
	sway_log(L_INFO, "buffer release");
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_release
};

struct buffer *create_buffer(struct client_state *state,
		int32_t width, int32_t height, uint32_t format) {

	struct buffer *buf = malloc(sizeof(struct buffer));
	memset(buf, 0, sizeof(struct buffer));
	uint32_t stride = width * 4;
	uint32_t size = stride * height;

	int fd = create_pool_file(size);
	void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	buf->pool = wl_shm_create_pool(state->shm, fd, size);
	buf->buffer = wl_shm_pool_create_buffer(buf->pool, 0, width, height, stride, format);
	wl_shm_pool_destroy(buf->pool);
	close(fd);
	fd = -1;

	state->cairo_surface = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_ARGB32, width, height, stride);
	state->cairo = cairo_create(state->cairo_surface);
	state->pango = pango_cairo_create_context(state->cairo);

	wl_buffer_add_listener(buf->buffer, &buffer_listener, state);

	sway_log(L_INFO, "%p %p", buf->pool, buf->buffer);
	return buf;
}

static void frame_callback(void *data, struct wl_callback *callback, uint32_t time) {
	sway_log(L_INFO, "frame callback");
	struct client_state *state = data;
	wl_callback_destroy(callback);
	state->frame_cb = NULL;
}

static const struct wl_callback_listener listener = {
	frame_callback
};

struct client_state *client_setup(void) {
	struct client_state *state = malloc(sizeof(struct client_state));
	memset(state, 0, sizeof(struct client_state));
	state->outputs = create_list();

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

	state->buffer = create_buffer(state, 100, 100, WL_SHM_FORMAT_ARGB8888);
	state->surface = wl_compositor_create_surface(state->compositor);
	state->shell_surface = wl_shell_get_shell_surface(state->shell, state->surface);
	wl_shell_surface_set_toplevel(state->shell_surface);

	wl_surface_damage(state->surface, 0, 0, 100, 100);

	return state;
}

int client_render(struct client_state *state) {
	if (state->frame_cb || state->busy) {
		return 2;
	}
	sway_log(L_INFO, "Rendering");

	state->frame_cb = wl_surface_frame(state->surface);
	wl_callback_add_listener(state->frame_cb, &listener, state);

	wl_surface_damage(state->surface, 0, 0, state->buffer->width, state->buffer->height);
	wl_surface_attach(state->surface, state->buffer->buffer, 0, 0);
	wl_surface_commit(state->surface);

	state->busy = true;

	return wl_display_dispatch(state->display) != -1;
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
