#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-client.h>
#include <wlr/util/log.h>
#include "background-image.h"
#include "pool-buffer.h"
#include "cairo.h"
#include "util.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct swaybg_args {
	int output_idx;
	const char *path;
	enum background_mode mode;
};

struct swaybg_context {
	uint32_t color;
	cairo_surface_t *image;
};

struct swaybg_state {
	const struct swaybg_args *args;
	struct swaybg_context context;

	struct wl_display *display;
	struct wl_compositor *compositor;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_shm *shm;

	struct wl_output *output;
	struct wl_surface *surface;
	struct wl_region *input_region;
	struct zwlr_layer_surface_v1 *layer_surface;

	bool run_display;
	uint32_t width, height;
	int32_t scale;
	struct pool_buffer buffers[2];
	struct pool_buffer *current_buffer;
};

bool is_valid_color(const char *color) {
	int len = strlen(color);
	if (len != 7 || color[0] != '#') {
		wlr_log(L_ERROR, "%s is not a valid color for swaybg. "
				"Color should be specified as #rrggbb (no alpha).", color);
		return false;
	}

	int i;
	for (i = 1; i < len; ++i) {
		if (!isxdigit(color[i])) {
			return false;
		}
	}

	return true;
}

static void render_frame(struct swaybg_state *state) {
	int buffer_width = state->width * state->scale,
		buffer_height = state->height * state->scale;
	state->current_buffer = get_next_buffer(state->shm,
			state->buffers, buffer_width, buffer_height);
	cairo_t *cairo = state->current_buffer->cairo;
	if (state->args->mode == BACKGROUND_MODE_SOLID_COLOR) {
		cairo_set_source_u32(cairo, state->context.color);
		cairo_paint(cairo);
	} else {
		render_background_image(cairo, state->context.image,
				state->args->mode, buffer_width, buffer_height);
	}

	wl_surface_set_buffer_scale(state->surface, state->scale);
	wl_surface_attach(state->surface, state->current_buffer->buffer, 0, 0);
	wl_surface_damage(state->surface, 0, 0, state->width, state->height);
	wl_surface_commit(state->surface);
}

static bool prepare_context(struct swaybg_state *state) {
	if (state->args->mode == BACKGROUND_MODE_SOLID_COLOR) {
		state->context.color = parse_color(state->args->path);
		return is_valid_color(state->args->path);
	}
	if (!(state->context.image = load_background_image(state->args->path))) {
		return false;
	}
	return true;
}

static void layer_surface_configure(void *data,
		struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t width, uint32_t height) {
	struct swaybg_state *state = data;
	state->width = width;
	state->height = height;
	zwlr_layer_surface_v1_ack_configure(surface, serial);
	render_frame(state);
}

static void layer_surface_closed(void *data,
		struct zwlr_layer_surface_v1 *surface) {
	struct swaybg_state *state = data;
	zwlr_layer_surface_v1_destroy(state->layer_surface);
	wl_surface_destroy(state->surface);
	wl_region_destroy(state->input_region);
	state->run_display = false;
}

struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

static void output_geometry(void *data, struct wl_output *output, int32_t x,
		int32_t y, int32_t width_mm, int32_t height_mm, int32_t subpixel,
		const char *make, const char *model, int32_t transform) {
	// Who cares
}

static void output_mode(void *data, struct wl_output *output, uint32_t flags,
		int32_t width, int32_t height, int32_t refresh) {
	// Who cares
}

static void output_done(void *data, struct wl_output *output) {
	// Who cares
}

static void output_scale(void *data, struct wl_output *output, int32_t factor) {
	struct swaybg_state *state = data;
	state->scale = factor;
	if (state->run_display) {
		render_frame(state);
	}
}

struct wl_output_listener output_listener = {
	.geometry = output_geometry,
	.mode = output_mode,
	.done = output_done,
	.scale = output_scale,
};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct swaybg_state *state = data;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->compositor = wl_registry_bind(registry, name,
				&wl_compositor_interface, 3);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->shm = wl_registry_bind(registry, name,
				&wl_shm_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		static int output_idx = 0;
		if (output_idx == state->args->output_idx) {
			state->output = wl_registry_bind(registry, name,
					&wl_output_interface, 3);
			wl_output_add_listener(state->output, &output_listener, state);
		}
		output_idx++;
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		state->layer_shell = wl_registry_bind(
				registry, name, &zwlr_layer_shell_v1_interface, 1);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	// who cares
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

int main(int argc, const char **argv) {
	struct swaybg_args args = {0};
	struct swaybg_state state = {0};
	state.args = &args;
	wlr_log_init(L_DEBUG, NULL);

	if (argc != 4) {
		wlr_log(L_ERROR, "Do not run this program manually. "
				"See man 5 sway and look for output options.");
		return 1;
	}
	args.output_idx = atoi(argv[1]);
	args.path = argv[2];

	args.mode = parse_background_mode(argv[3]);
	if (args.mode == BACKGROUND_MODE_INVALID) {
		return 1;
	}
	if (!prepare_context(&state)) {
		return 1;
	}

	assert(state.display = wl_display_connect(NULL));

	struct wl_registry *registry = wl_display_get_registry(state.display);
	wl_registry_add_listener(registry, &registry_listener, &state);
	wl_display_roundtrip(state.display);
	assert(state.compositor && state.layer_shell && state.output && state.shm);

	// Second roundtrip to get output properties
	wl_display_roundtrip(state.display);

	assert(state.surface = wl_compositor_create_surface(state.compositor));

	assert(state.input_region = wl_compositor_create_region(state.compositor));
	wl_surface_set_input_region(state.surface, state.input_region);

	state.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
			state.layer_shell, state.surface, state.output,
			ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "wallpaper");
	assert(state.layer_surface);

	zwlr_layer_surface_v1_set_size(state.layer_surface, 0, 0);
	zwlr_layer_surface_v1_set_anchor(state.layer_surface,
			ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
	zwlr_layer_surface_v1_set_exclusive_zone(state.layer_surface, -1);
	zwlr_layer_surface_v1_add_listener(state.layer_surface,
			&layer_surface_listener, &state);
	wl_surface_commit(state.surface);
	wl_display_roundtrip(state.display);

	state.run_display = true;
	while (wl_display_dispatch(state.display) != -1 && state.run_display) {
		// This space intentionally left blank
	}
	return 0;
}
