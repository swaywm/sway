#ifndef _SWAY_DECORATION_H
#define _SWAY_DECORATION_H

#include <wlr/types/wlr_server_decoration.h>

struct sway_server_decoration {
	struct wlr_server_decoration *wlr_server_decoration;
	struct wl_list link;

	struct wl_listener destroy;
	struct wl_listener mode;
};

struct sway_server_decoration *decoration_from_surface(
	struct wlr_surface *surface);

#endif
