#ifndef _SWAY_LAYERS_H
#define _SWAY_LAYERS_H
#include <stdbool.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/types/wlr_layer_shell.h>

struct sway_layer_surface {
	struct wlr_layer_surface *layer_surface;
	struct wl_list link;

	struct wl_listener destroy;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener surface_commit;
	struct wl_listener output_destroy;
	struct wl_listener output_mode;
	struct wl_listener output_transform;

	bool configured;
	struct wlr_box geo;
};

#endif
