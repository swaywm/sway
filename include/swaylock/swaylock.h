#ifndef _SWAYLOCK_H
#define _SWAYLOCK_H
#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include "background-image.h"
#include "cairo.h"
#include "pool-buffer.h"
#include "swaylock/seat.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct swaylock_args {
	uint32_t color;
	enum background_mode mode;
	bool show_indicator;
};

struct swaylock_state {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_shm *shm;
	struct wl_list contexts;
	struct swaylock_args args;
	struct swaylock_xkb xkb;
	bool run_display;
};

struct swaylock_context {
	cairo_surface_t *image;
	struct swaylock_state *state;
	struct wl_output *output;
	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct pool_buffer buffers[2];
	struct pool_buffer *current_buffer;
	uint32_t width, height;
	struct wl_list link;
};

#endif
