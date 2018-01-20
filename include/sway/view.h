#ifndef _SWAY_VIEW_H
#define _SWAY_VIEW_H
#include <wayland-server.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/xwayland.h>

struct sway_container;
struct sway_view;

struct sway_xdg_surface_v6 {
	struct sway_view *view;

	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener destroy;

	int pending_width, pending_height;
};

struct sway_xwayland_surface {
	struct sway_view *view;

	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_configure;
	struct wl_listener unmap_notify;
	struct wl_listener map_notify;
	struct wl_listener destroy;

	int pending_width, pending_height;
};

struct sway_wl_shell_surface {
	struct sway_view *view;

	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener destroy;

	int pending_width, pending_height;
};

enum sway_view_type {
	SWAY_WL_SHELL_VIEW,
	SWAY_XDG_SHELL_V6_VIEW,
	SWAY_XWAYLAND_VIEW,
	// Keep last
	SWAY_VIEW_TYPES,
};

enum sway_view_prop {
	VIEW_PROP_TITLE,
	VIEW_PROP_APP_ID,
	VIEW_PROP_CLASS,
	VIEW_PROP_INSTANCE,
};

/**
 * sway_view is a state container for surfaces that are arranged in the sway
 * tree (shell surfaces).
 */
struct sway_view {
	enum sway_view_type type;
	struct sway_container *swayc;
	struct wlr_surface *surface;
	int width, height;

	union {
		struct wlr_xdg_surface_v6 *wlr_xdg_surface_v6;
		struct wlr_xwayland_surface *wlr_xwayland_surface;
		struct wlr_wl_shell_surface *wlr_wl_shell_surface;
	};

	union {
		struct sway_xdg_surface_v6 *sway_xdg_surface_v6;
		struct sway_xwayland_surface *sway_xwayland_surface;
		struct sway_wl_shell_surface *sway_wl_shell_surface;
	};

	struct {
		const char *(*get_prop)(struct sway_view *view,
				enum sway_view_prop prop);
		void (*set_size)(struct sway_view *view,
				int width, int height);
		void (*set_position)(struct sway_view *view,
				double ox, double oy);
		void (*set_activated)(struct sway_view *view, bool activated);
		void (*close)(struct sway_view *view);
	} iface;

	// only used for unmanaged views (shell specific)
	struct wl_list unmanaged_view_link; // sway_root::unmanaged views
};

#endif
