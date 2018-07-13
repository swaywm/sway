#ifndef _SWAYBG_H
#define _SWAYBG_H
#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include "background-image.h"
#include "cairo.h"
#include "pool-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct swaybg_args {
	uint32_t color;
	enum background_mode mode;
};

// There is exactly one swaybg_image for each -i argument
struct swaybg_image {
	char *path;
	char *output_name;
	cairo_surface_t *cairo_surface;
	struct wl_list link;
};

struct swaybg_state {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_shm *shm;
	struct wl_list surfaces;
	struct wl_list images;
	struct swaybg_args args;
	bool run_display;
	struct zxdg_output_manager_v1 *zxdg_output_manager;
};

struct swaybg_surface {
	cairo_surface_t *image;
	struct swaybg_state *state;
	struct wl_output *output;
	uint32_t output_global_name;
	struct zxdg_output_v1 *xdg_output;
	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct pool_buffer buffers[2];
	struct pool_buffer *current_buffer;
	uint32_t width, height;
	int32_t scale;
	char *output_name;
	struct wl_list link;
};

void render_surface(struct swaybg_surface *surface);

#endif
