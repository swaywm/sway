#ifndef _SWAY_XDG_DECORATION_H
#define _SWAY_XDG_DECORATION_H

#include <wlr/types/wlr_xdg_decoration_v1.h>

struct sway_xdg_decoration {
	struct wlr_xdg_toplevel_decoration_v1 *wlr_xdg_decoration;
	struct wl_list link;

	struct sway_view *view;

	struct wl_listener destroy;
	struct wl_listener request_mode;
};

struct sway_xdg_decoration *xdg_decoration_from_surface(
	struct wlr_surface *surface);

#endif
