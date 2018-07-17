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
	SWAY_VIEW_XDG_SHELL_V6,
	SWAY_VIEW_XDG_SHELL,
	SWAY_VIEW_XWAYLAND,
};

enum sway_view_prop {
	VIEW_PROP_TITLE,
	VIEW_PROP_APP_ID,
	VIEW_PROP_CLASS,
	VIEW_PROP_INSTANCE,
	VIEW_PROP_WINDOW_TYPE,
	VIEW_PROP_WINDOW_ROLE,
	VIEW_PROP_X11_WINDOW_ID,
};

struct sway_view_impl {
	const char *(*get_string_prop)(struct sway_view *view,
			enum sway_view_prop prop);
	uint32_t (*get_int_prop)(struct sway_view *view, enum sway_view_prop prop);
	uint32_t (*configure)(struct sway_view *view, double lx, double ly,
			int width, int height);
	void (*set_activated)(struct sway_view *view, bool activated);
	void (*set_tiled)(struct sway_view *view, bool tiled);
	void (*set_fullscreen)(struct sway_view *view, bool fullscreen);
	bool (*wants_floating)(struct sway_view *view);
	bool (*has_client_side_decorations)(struct sway_view *view);
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

	// Geometry of the view itself (excludes borders) in layout coordinates
	double x, y;
	int width, height;

	double saved_x, saved_y;
	int saved_width, saved_height;

	// The size the view would want to be if it weren't tiled.
	// Used when changing a view from tiled to floating.
	int natural_width, natural_height;

	bool is_fullscreen;

	char *title_format;
	enum sway_container_border border;
	int border_thickness;
	bool border_top;
	bool border_bottom;
	bool border_left;
	bool border_right;
	bool using_csd;

	struct timespec urgent;
	bool allow_request_urgent;
	struct wl_event_source *urgent_timer;

	bool destroying;

	list_t *executed_criteria; // struct criteria *
	list_t *marks;             // char *

	struct wlr_texture *marks_focused;
	struct wlr_texture *marks_focused_inactive;
	struct wlr_texture *marks_unfocused;
	struct wlr_texture *marks_urgent;

	union {
		struct wlr_xdg_surface_v6 *wlr_xdg_surface_v6;
		struct wlr_xdg_surface *wlr_xdg_surface;
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
};

struct sway_xdg_shell_view {
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
};

struct sway_xwayland_view {
	struct sway_view view;

	struct wl_listener commit;
	struct wl_listener request_move;
	struct wl_listener request_resize;
	struct wl_listener request_maximize;
	struct wl_listener request_configure;
	struct wl_listener request_fullscreen;
	struct wl_listener set_title;
	struct wl_listener set_class;
	struct wl_listener set_window_type;
	struct wl_listener map;
	struct wl_listener unmap;
	struct wl_listener destroy;
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
	struct wl_listener destroy;
};

struct sway_xdg_popup {
	struct sway_view_child child;

	struct wlr_xdg_surface *wlr_xdg_surface;

	struct wl_listener new_popup;
	struct wl_listener destroy;
};

const char *view_get_title(struct sway_view *view);

const char *view_get_app_id(struct sway_view *view);

const char *view_get_class(struct sway_view *view);

const char *view_get_instance(struct sway_view *view);

uint32_t view_get_x11_window_id(struct sway_view *view);

const char *view_get_window_role(struct sway_view *view);

uint32_t view_get_window_type(struct sway_view *view);

const char *view_get_shell(struct sway_view *view);

uint32_t view_configure(struct sway_view *view, double lx, double ly, int width,
	int height);

/**
 * Center the view in its workspace and build the swayc decorations around it.
 */
void view_init_floating(struct sway_view *view);

/**
 * Configure the view's position and size based on the swayc's position and
 * size, taking borders into consideration.
 */
void view_autoconfigure(struct sway_view *view);

void view_set_activated(struct sway_view *view, bool activated);

void view_set_tiled(struct sway_view *view, bool tiled);

void view_set_fullscreen_raw(struct sway_view *view, bool fullscreen);

void view_set_fullscreen(struct sway_view *view, bool fullscreen);

void view_close(struct sway_view *view);

void view_damage_from(struct sway_view *view);

void view_for_each_surface(struct sway_view *view,
	wlr_surface_iterator_func_t iterator, void *user_data);

// view implementation

void view_init(struct sway_view *view, enum sway_view_type type,
	const struct sway_view_impl *impl);

void view_free(struct sway_view *view);

void view_destroy(struct sway_view *view);

void view_map(struct sway_view *view, struct wlr_surface *wlr_surface);

void view_unmap(struct sway_view *view);

void view_update_position(struct sway_view *view, double lx, double ly);

void view_update_size(struct sway_view *view, int width, int height);

void view_child_init(struct sway_view_child *child,
	const struct sway_view_child_impl *impl, struct sway_view *view,
	struct wlr_surface *surface);

void view_child_destroy(struct sway_view_child *child);


struct sway_view *view_from_wlr_xdg_surface(
	struct wlr_xdg_surface *xdg_surface);
struct sway_view *view_from_wlr_xdg_surface_v6(
	struct wlr_xdg_surface_v6 *xdg_surface_v6);
struct sway_view *view_from_wlr_xwayland_surface(
	struct wlr_xwayland_surface *xsurface);

struct sway_view *view_from_wlr_surface(struct wlr_surface *surface);

/**
 * Re-read the view's title property and update any relevant title bars.
 * The force argument makes it recreate the title bars even if the title hasn't
 * changed.
 */
void view_update_title(struct sway_view *view, bool force);

/**
 * Run any criteria that match the view and haven't been run on this view
 * before.
 */
void view_execute_criteria(struct sway_view *view);

/**
 * Find any view that has the given mark and remove the mark from the view.
 * Returns true if it matched a view.
 */
bool view_find_and_unmark(char *mark);

/**
 * Remove all marks from the view.
 */
void view_clear_marks(struct sway_view *view);

bool view_has_mark(struct sway_view *view, char *mark);

void view_update_marks_textures(struct sway_view *view);

/**
 * Returns true if there's a possibility the view may be rendered on screen.
 * Intended for damage tracking.
 */
bool view_is_visible(struct sway_view *view);

void view_set_urgent(struct sway_view *view, bool enable);

bool view_is_urgent(struct sway_view *view);

#endif
