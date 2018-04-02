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

static bool assert_xdg(struct sway_view *view) {
	return sway_assert(view->type == SWAY_XDG_SHELL_V6_VIEW,
			"Expected xdg shell v6 view!");
}

static const char *get_prop(struct sway_view *view, enum sway_view_prop prop) {
	if (!assert_xdg(view)) {
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
	if (!assert_xdg(view)) {
		return;
	}

	view_update_position(view, ox, oy);
	view->sway_xdg_surface_v6->pending_width = width;
	view->sway_xdg_surface_v6->pending_height = height;
	wlr_xdg_toplevel_v6_set_size(view->wlr_xdg_surface_v6, width, height);
}

static void set_activated(struct sway_view *view, bool activated) {
	if (!assert_xdg(view)) {
		return;
	}
	struct wlr_xdg_surface_v6 *surface = view->wlr_xdg_surface_v6;
	if (surface->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
		wlr_xdg_toplevel_v6_set_activated(surface, activated);
	}
}

static void _close(struct sway_view *view) {
	if (!assert_xdg(view)) {
		return;
	}
	struct wlr_xdg_surface_v6 *surface = view->wlr_xdg_surface_v6;
	if (surface->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
		wlr_xdg_surface_v6_send_close(surface);
	}
}

static const struct sway_view_impl view_impl = {
	.get_prop = get_prop,
	.configure = configure,
	.set_activated = set_activated,
	.close = _close,
};

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_xdg_surface_v6 *sway_surface =
		wl_container_of(listener, sway_surface, commit);
	struct sway_view *view = sway_surface->view;
	// NOTE: We intentionally discard the view's desired width here
	// TODO: Store this for restoration when moving to floating plane
	// TODO: Let floating views do whatever
	view_update_size(view, sway_surface->pending_width,
		sway_surface->pending_height);
	view_damage_from(view);
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_xdg_surface_v6 *sway_surface =
		wl_container_of(listener, sway_surface, unmap);
	view_unmap(sway_surface->view);
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct sway_xdg_surface_v6 *sway_surface =
		wl_container_of(listener, sway_surface, map);
	struct sway_view *view = sway_surface->view;
	view_map(view, view->wlr_xdg_surface_v6->surface);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xdg_surface_v6 *sway_xdg_surface =
		wl_container_of(listener, sway_xdg_surface, destroy);
	wl_list_remove(&sway_xdg_surface->commit.link);
	wl_list_remove(&sway_xdg_surface->destroy.link);
	wl_list_remove(&sway_xdg_surface->map.link);
	wl_list_remove(&sway_xdg_surface->unmap.link);
	view_destroy(sway_xdg_surface->view);
	free(sway_xdg_surface);
}

void handle_xdg_shell_v6_surface(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(
			listener, server, xdg_shell_v6_surface);
	struct wlr_xdg_surface_v6 *xdg_surface = data;

	if (xdg_surface->role == WLR_XDG_SURFACE_V6_ROLE_POPUP) {
		// TODO: popups
		return;
	}

	wlr_log(L_DEBUG, "New xdg_shell_v6 toplevel title='%s' app_id='%s'",
			xdg_surface->toplevel->title, xdg_surface->toplevel->app_id);
	wlr_xdg_surface_v6_ping(xdg_surface);
	wlr_xdg_toplevel_v6_set_maximized(xdg_surface, true);

	struct sway_xdg_surface_v6 *sway_surface =
		calloc(1, sizeof(struct sway_xdg_surface_v6));
	if (!sway_assert(sway_surface, "Failed to allocate surface!")) {
		return;
	}

	struct sway_view *view = view_create(SWAY_XDG_SHELL_V6_VIEW, &view_impl);
	if (!sway_assert(view, "Failed to allocate view")) {
		return;
	}
	view->wlr_xdg_surface_v6 = xdg_surface;
	view->sway_xdg_surface_v6 = sway_surface;
	sway_surface->view = view;

	// TODO:
	// - Look up pid and open on appropriate workspace
	// - Criteria

	sway_surface->commit.notify = handle_commit;
	wl_signal_add(&xdg_surface->surface->events.commit, &sway_surface->commit);

	sway_surface->map.notify = handle_map;
	wl_signal_add(&xdg_surface->events.map, &sway_surface->map);

	sway_surface->unmap.notify = handle_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &sway_surface->unmap);

	sway_surface->destroy.notify = handle_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &sway_surface->destroy);
}
