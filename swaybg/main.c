#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-client.h>
#include <wlr/util/log.h>
#include "buffer_pool.h"
#include "cairo.h"
#include "util.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

enum scaling_mode {
	SCALING_MODE_STRETCH,
	SCALING_MODE_FILL,
	SCALING_MODE_FIT,
	SCALING_MODE_CENTER,
	SCALING_MODE_TILE,
	SCALING_MODE_SOLID_COLOR,
};

struct swaybg_args {
	int output_idx;
	const char *path;
	enum scaling_mode mode;
};

struct swaybg_state {
	const struct swaybg_args *args;

	struct wl_display *display;
	struct wl_compositor *compositor;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_shm *shm;

	struct wl_output *output;
	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;

	bool run_display;
	uint32_t width, height;
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
	if (!state->run_display) {
		return;
	}

	state->current_buffer = get_next_buffer(state->shm,
			state->buffers, state->width, state->height);
	cairo_t *cairo = state->current_buffer->cairo;

	switch (state->args->mode) {
	case SCALING_MODE_SOLID_COLOR:
		cairo_set_source_u32(cairo, parse_color(state->args->path));
		cairo_paint(cairo);
		break;
	default:
		exit(1);
		break;
	}

	wl_surface_attach(state->surface, state->current_buffer->buffer, 0, 0);
	wl_surface_damage(state->surface, 0, 0, state->width, state->height);
	wl_surface_commit(state->surface);
}

static void layer_surface_configure(void *data,
		struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t width, uint32_t height) {
	struct swaybg_state *state = data;
	state->width = width;
	state->height = height;
	render_frame(state);
	zwlr_layer_surface_v1_ack_configure(surface, serial);
}

static void layer_surface_closed(void *data,
		struct zwlr_layer_surface_v1 *surface) {
	struct swaybg_state *state = data;
	zwlr_layer_surface_v1_destroy(state->layer_surface);
	wl_surface_destroy(state->surface);
	state->run_display = false;
}

struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct swaybg_state *state = data;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->compositor = wl_registry_bind(registry, name,
				&wl_compositor_interface, 1);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->shm = wl_registry_bind(registry, name,
				&wl_shm_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		static int output_idx = 0;
		if (output_idx == state->args->output_idx) {
			state->output = wl_registry_bind(registry, name,
					&wl_output_interface, 1);
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
	args.mode = atoi(argv[3]);

	args.mode = SCALING_MODE_STRETCH;
	if (strcmp(argv[3], "stretch") == 0) {
		args.mode = SCALING_MODE_STRETCH;
	} else if (strcmp(argv[3], "fill") == 0) {
		args.mode = SCALING_MODE_FILL;
	} else if (strcmp(argv[3], "fit") == 0) {
		args.mode = SCALING_MODE_FIT;
	} else if (strcmp(argv[3], "center") == 0) {
		args.mode = SCALING_MODE_CENTER;
	} else if (strcmp(argv[3], "tile") == 0) {
		args.mode = SCALING_MODE_TILE;
	} else if (strcmp(argv[3], "solid_color") == 0) {
		args.mode = SCALING_MODE_SOLID_COLOR;
	} else {
		wlr_log(L_ERROR, "Unsupported scaling mode: %s", argv[3]);
		return 1;
	}

	state.display = wl_display_connect(NULL);
	if (!state.display) {
		wlr_log(L_ERROR, "Failed to create display\n");
		return 1;
	}

	struct wl_registry *registry = wl_display_get_registry(state.display);
	wl_registry_add_listener(registry, &registry_listener, &state);
	wl_display_roundtrip(state.display);

	if (!state.compositor) {
		wlr_log(L_DEBUG, "wl-compositor not available");
		return 1;
	}
	if (!state.layer_shell) {
		wlr_log(L_ERROR, "layer-shell not available");
		return 1;
	}

	state.surface = wl_compositor_create_surface(state.compositor);
	if (!state.surface) {
		wlr_log(L_ERROR, "failed to create wl_surface");
		return 1;
	}

	state.layer_surface = zwlr_layer_shell_v1_get_layer_surface(
			state.layer_shell, state.surface, state.output,
			ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "wallpaper");
	if (!state.layer_surface) {
		wlr_log(L_ERROR, "failed to create zwlr_layer_surface");
		return 1;
	}
	zwlr_layer_surface_v1_set_size(state.layer_surface, 0, 0);
	zwlr_layer_surface_v1_set_anchor(state.layer_surface,
			ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
	zwlr_layer_surface_v1_add_listener(state.layer_surface,
			&layer_surface_listener, &state);
	wl_surface_commit(state.surface);
	wl_display_roundtrip(state.display);

	state.run_display = true;
	render_frame(&state);
	while (wl_display_dispatch(state.display) != -1 && state.run_display) {
		// This space intentionally left blank
	}
	return 0;
}
