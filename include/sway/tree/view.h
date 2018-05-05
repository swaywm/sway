#ifndef _SWAY_VIEW_H
#define _SWAY_VIEW_H
#include <wayland-server.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/xwayland.h>
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"

struct sway_container;

enum sway_view_type {
	SWAY_VIEW_WL_SHELL,
	SWAY_VIEW_XDG_SHELL_V6,
	SWAY_VIEW_XWAYLAND,
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
	void (*set_fullscreen)(struct sway_view *view, bool fullscreen);
	void (*for_each_surface)(struct sway_view *view,
		wlr_surface_iterator_func_t iterator, void *user_data);
	void (*close)(struct sway_view *view);
	void (*destroy)(struct sway_view *view);
};

struct sway_view {
	enum sway_view_type type;
	const struct sway_view_impl *impl;

	struct sway_container *swayc; // NULL for unmapped views
	struct wlr_surface *surface; // NULL for unmapped views

	// Geometry of the view itself (excludes borders)
	double x, y;
	int width, height;

	bool is_fullscreen;

	char *title_format;
	enum sway_container_border border;
	int border_thickness;

	union {
		struct wlr_xdg_surface_v6 *wlr_xdg_surface_v6;
		struct wlr_xwayland_surface *wlr_xwayland_surface;
		struct wlr_wl_shell_surface *wlr_wl_shell_surface;
	};

	struct {
		struct wl_signal unmap;
	} events;

	struct wl_listener surface_new_subsurface;
	struct wl_listener container_reparent;
};

struct sway_xdg_shell_v6_view {
	struct sway_view view;

	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener new_popup;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;

	int pending_width, pending_height;
};

struct sway_xwayland_view {
	struct sway_view view;

	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_configure;
	struct wl_listener request_fullscreen;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;

	int pending_width, pending_height;
};

struct sway_xwayland_unmanaged {
	struct wlr_xwayland_surface *wlr_xwayland_surface;
	struct wl_list link;

	int lx, ly;

	struct wl_listener request_configure;
	struct wl_listener request_fullscreen;
	struct wl_listener commit;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
};

struct sway_wl_shell_view {
	struct sway_view view;

	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_fullscreen;
	struct wl_listener set_state;
	struct wl_listener destroy;

	int pending_width, pending_height;
};

struct sway_view_child;

struct sway_view_child_impl {
	void (*destroy)(struct sway_view_child *child);
};

/**
 * A view child is a surface in the view tree, such as a subsurface or a popup.
 */
struct sway_view_child {
	const struct sway_view_child_impl *impl;

	struct sway_view *view;
	struct wlr_surface *surface;

	struct wl_listener surface_commit;
	struct wl_listener surface_new_subsurface;
	struct wl_listener surface_destroy;
	struct wl_listener view_unmap;
};

struct sway_xdg_popup_v6 {
	struct sway_view_child child;

	struct wlr_xdg_surface_v6 *wlr_xdg_surface_v6;

	struct wl_listener new_popup;
	struct wl_listener unmap;
	struct wl_listener destroy;
};

const char *view_get_title(struct sway_view *view);

const char *view_get_app_id(struct sway_view *view);

const char *view_get_class(struct sway_view *view);

const char *view_get_instance(struct sway_view *view);

const char *view_get_type(struct sway_view *view);

void view_configure(struct sway_view *view, double ox, double oy, int width,
	int height);

/**
 * Configure the view's position and size based on the swayc's position and
 * size, taking borders into consideration.
 */
void view_autoconfigure(struct sway_view *view);

void view_set_activated(struct sway_view *view, bool activated);

void view_set_fullscreen_raw(struct sway_view *view, bool fullscreen);

void view_set_fullscreen(struct sway_view *view, bool fullscreen);

void view_close(struct sway_view *view);

void view_damage(struct sway_view *view, bool whole);

void view_for_each_surface(struct sway_view *view,
	wlr_surface_iterator_func_t iterator, void *user_data);

// view implementation

void view_init(struct sway_view *view, enum sway_view_type type,
	const struct sway_view_impl *impl);

void view_destroy(struct sway_view *view);

void view_map(struct sway_view *view, struct wlr_surface *wlr_surface);

void view_unmap(struct sway_view *view);

void view_update_position(struct sway_view *view, double ox, double oy);

void view_update_size(struct sway_view *view, int width, int height);

void view_child_init(struct sway_view_child *child,
	const struct sway_view_child_impl *impl, struct sway_view *view,
	struct wlr_surface *surface);

void view_child_destroy(struct sway_view_child *child);

/**
 * Re-read the view's title property and update any relevant title bars.
 * The force argument makes it recreate the title bars even if the title hasn't
 * changed.
 */
void view_update_title(struct sway_view *view, bool force);

#endif
