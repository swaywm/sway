#ifndef _SWAY_VIEW_H
#define _SWAY_VIEW_H
#include <wayland-server.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/xwayland.h>
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"

struct sway_container;
struct sway_view;

struct sway_xdg_surface_v6 {
	struct sway_view *view;

	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener map;
	struct wl_listener unmap;
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
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;

	int pending_width, pending_height;
};

struct sway_xwayland_unmanaged {
	struct wlr_xwayland_surface *wlr_xwayland_surface;
	struct wl_list link;

	struct wl_listener destroy;
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

struct sway_view_impl {
	const char *(*get_prop)(struct sway_view *view,
			enum sway_view_prop prop);
	void (*configure)(struct sway_view *view, double ox, double oy, int width,
		int height);
	void (*set_activated)(struct sway_view *view, bool activated);
	void (*close)(struct sway_view *view);
};

struct sway_view {
	enum sway_view_type type;
	const struct sway_view_impl *impl;

	struct sway_container *swayc; // NULL for unmanaged views
	struct wlr_surface *surface; // NULL for unmapped views
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
};

struct sway_view *view_create(enum sway_view_type type,
	const struct sway_view_impl *impl);

void view_destroy(struct sway_view *view);

const char *view_get_title(struct sway_view *view);

const char *view_get_app_id(struct sway_view *view);

const char *view_get_class(struct sway_view *view);

const char *view_get_instance(struct sway_view *view);

void view_configure(struct sway_view *view, double ox, double oy, int width,
	int height);

void view_set_activated(struct sway_view *view, bool activated);

void view_close(struct sway_view *view);

void view_damage_whole(struct sway_view *view);

void view_damage_from(struct sway_view *view);

// view implementation

void view_map(struct sway_view *view, struct wlr_surface *wlr_surface);

void view_unmap(struct sway_view *view);

void view_update_position(struct sway_view *view, double ox, double oy);

void view_update_size(struct sway_view *view, int width, int height);

#endif
