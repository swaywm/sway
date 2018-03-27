#define _POSIX_C_SOURCE 199309L
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include "sway/container.h"
#include "sway/layout.h"
#include "sway/server.h"
#include "sway/view.h"
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

static void set_size(struct sway_view *view, int width, int height) {
	if (!assert_xdg(view)) {
		return;
	}
	view->sway_xdg_surface_v6->pending_width = width;
	view->sway_xdg_surface_v6->pending_height = height;
	wlr_xdg_toplevel_v6_set_size(view->wlr_xdg_surface_v6, width, height);
}

static void set_position(struct sway_view *view, double ox, double oy) {
	if (!assert_xdg(view)) {
		return;
	}
	view->swayc->x = ox;
	view->swayc->y = oy;
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

static void close(struct sway_view *view) {
	if (!assert_xdg(view)) {
		return;
	}
	struct wlr_xdg_surface_v6 *surface = view->wlr_xdg_surface_v6;
	if (surface->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
		wlr_xdg_surface_v6_send_close(surface);
	}
}

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_xdg_surface_v6 *sway_surface =
		wl_container_of(listener, sway_surface, commit);
	struct sway_view *view = sway_surface->view;
	// NOTE: We intentionally discard the view's desired width here
	// TODO: Store this for restoration when moving to floating plane
	// TODO: Let floating views do whatever
	view->width = sway_surface->pending_width;
	view->height = sway_surface->pending_height;
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xdg_surface_v6 *sway_xdg_surface =
		wl_container_of(listener, sway_xdg_surface, destroy);
	wl_list_remove(&sway_xdg_surface->commit.link);
	wl_list_remove(&sway_xdg_surface->destroy.link);
	swayc_t *parent = destroy_view(sway_xdg_surface->view->swayc);
	free(sway_xdg_surface->view);
	free(sway_xdg_surface);
	arrange_windows(parent, -1, -1);
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

	struct sway_xdg_surface_v6 *sway_surface =
		calloc(1, sizeof(struct sway_xdg_surface_v6));
	if (!sway_assert(sway_surface, "Failed to allocate surface!")) {
		return;
	}

	struct sway_view *sway_view = calloc(1, sizeof(struct sway_view));
	if (!sway_assert(sway_view, "Failed to allocate view!")) {
		return;
	}
	sway_view->type = SWAY_XDG_SHELL_V6_VIEW;
	sway_view->iface.get_prop = get_prop;
	sway_view->iface.set_size = set_size;
	sway_view->iface.set_position = set_position;
	sway_view->iface.set_activated = set_activated;
	sway_view->iface.close = close;
	sway_view->wlr_xdg_surface_v6 = xdg_surface;
	sway_view->sway_xdg_surface_v6 = sway_surface;
	sway_view->surface = xdg_surface->surface;
	sway_surface->view = sway_view;
	
	// TODO:
	// - Look up pid and open on appropriate workspace
	// - Set new view to maximized so it behaves nicely
	// - Criteria
	
	sway_surface->commit.notify = handle_commit;
	wl_signal_add(&xdg_surface->surface->events.commit, &sway_surface->commit);

	sway_surface->destroy.notify = handle_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &sway_surface->destroy);

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	swayc_t *focus = sway_seat_get_focus_inactive(seat, &root_container);
	swayc_t *cont = new_view(focus, sway_view);
	sway_view->swayc = cont;

	arrange_windows(cont->parent, -1, -1);

	sway_input_manager_set_focus(input_manager, cont);
}
