#ifndef _SWAY_SURFACE_H
#define _SWAY_SURFACE_H
#include <wlr/types/wlr_surface.h>

struct sway_surface {
	struct wlr_surface *wlr_surface;

	struct wl_listener destroy;
};

#endif
