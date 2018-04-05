#define _POSIX_C_SOURCE 199309L
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/server.h"
#include "sway/tree/view.h"
#include "sway/input/seat.h"
#include "sway/input/input-manager.h"
#include "log.h"

static struct sway_xdg_shell_v6_view *xdg_shell_v6_view_from_view(
		struct sway_view *view) {
	if (!sway_assert(view->type == SWAY_VIEW_XDG_SHELL_V6,
			"Expected xdg_shell_v6 view")) {
		return NULL;
	}
	return (struct sway_xdg_shell_v6_view *)view;
}

static const char *get_prop(struct sway_view *view, enum sway_view_prop prop) {
	if (xdg_shell_v6_view_from_view(view) == NULL) {
		return NULL;
	}
	switch (prop) {
	case VIEW_PROP_TITLE:
		return view->wlr_xdg_surface_v6->toplevel->title;
	case VIEW_PROP_APP_ID:
		return view->wlr_xdg_surface_v6->toplevel->app_id;
	default:
		return NULL;
	}
}

static void configure(struct sway_view *view, double ox, double oy, int width,
		int height) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		xdg_shell_v6_view_from_view(view);
	if (xdg_shell_v6_view == NULL) {
		return;
	}

	view_update_position(view, ox, oy);
	xdg_shell_v6_view->pending_width = width;
	xdg_shell_v6_view->pending_height = height;
	wlr_xdg_toplevel_v6_set_size(view->wlr_xdg_surface_v6, width, height);
}

static void set_activated(struct sway_view *view, bool activated) {
	if (xdg_shell_v6_view_from_view(view) == NULL) {
		return;
	}
	struct wlr_xdg_surface_v6 *surface = view->wlr_xdg_surface_v6;
	if (surface->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
		wlr_xdg_toplevel_v6_set_activated(surface, activated);
	}
}

static void _close(struct sway_view *view) {
	if (xdg_shell_v6_view_from_view(view) == NULL) {
		return;
	}
	struct wlr_xdg_surface_v6 *surface = view->wlr_xdg_surface_v6;
	if (surface->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
		wlr_xdg_surface_v6_send_close(surface);
	}
}

static void destroy(struct sway_view *view) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		xdg_shell_v6_view_from_view(view);
	if (xdg_shell_v6_view == NULL) {
		return;
	}
	wl_list_remove(&xdg_shell_v6_view->commit.link);
	wl_list_remove(&xdg_shell_v6_view->destroy.link);
	wl_list_remove(&xdg_shell_v6_view->map.link);
	wl_list_remove(&xdg_shell_v6_view->unmap.link);
	free(xdg_shell_v6_view);
}

static const struct sway_view_impl view_impl = {
	.get_prop = get_prop,
	.configure = configure,
	.set_activated = set_activated,
	.close = _close,
};

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, commit);
	struct sway_view *view = &xdg_shell_v6_view->view;
	// NOTE: We intentionally discard the view's desired width here
	// TODO: Store this for restoration when moving to floating plane
	// TODO: Let floating views do whatever
	view_update_size(view, xdg_shell_v6_view->pending_width,
		xdg_shell_v6_view->pending_height);
	view_damage_from(view);
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, unmap);
	view_unmap(&xdg_shell_v6_view->view);
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, map);
	struct sway_view *view = &xdg_shell_v6_view->view;
	view_map(view, view->wlr_xdg_surface_v6->surface);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, destroy);
	view_destroy(&xdg_shell_v6_view->view);
}

void handle_xdg_shell_v6_surface(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(listener, server,
		xdg_shell_v6_surface);
	struct wlr_xdg_surface_v6 *xdg_surface = data;

	if (xdg_surface->role == WLR_XDG_SURFACE_V6_ROLE_POPUP) {
		wlr_log(L_DEBUG, "New xdg_shell_v6 popup");
		return;
	}

	wlr_log(L_DEBUG, "New xdg_shell_v6 toplevel title='%s' app_id='%s'",
		xdg_surface->toplevel->title, xdg_surface->toplevel->app_id);
	wlr_xdg_surface_v6_ping(xdg_surface);
	wlr_xdg_toplevel_v6_set_maximized(xdg_surface, true);

	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		calloc(1, sizeof(struct sway_xdg_shell_v6_view));
	if (!sway_assert(xdg_shell_v6_view, "Failed to allocate view")) {
		return;
	}

	view_init(&xdg_shell_v6_view->view, SWAY_VIEW_XDG_SHELL_V6, &view_impl);
	xdg_shell_v6_view->view.wlr_xdg_surface_v6 = xdg_surface;

	// TODO:
	// - Look up pid and open on appropriate workspace
	// - Criteria

	xdg_shell_v6_view->commit.notify = handle_commit;
	wl_signal_add(&xdg_surface->surface->events.commit,
		&xdg_shell_v6_view->commit);

	xdg_shell_v6_view->map.notify = handle_map;
	wl_signal_add(&xdg_surface->events.map, &xdg_shell_v6_view->map);

	xdg_shell_v6_view->unmap.notify = handle_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &xdg_shell_v6_view->unmap);

	xdg_shell_v6_view->destroy.notify = handle_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &xdg_shell_v6_view->destroy);
}
