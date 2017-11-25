#define _POSIX_C_SOURCE 199309L
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include "sway/container.h"
#include "sway/layout.h"
#include "sway/server.h"
#include "sway/view.h"
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
		return view->wlr_xdg_surface_v6->title;
	case VIEW_PROP_APP_ID:
		return view->wlr_xdg_surface_v6->app_id;
	default:
		return NULL;
	}
}

static void set_dimensions(struct sway_view *view, int width, int height) {
	if (assert_xdg(view)) {
		wlr_xdg_toplevel_v6_set_size(view->wlr_xdg_surface_v6, width, height);
	}
}

void handle_xdg_shell_v6_surface(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(
			listener, server, xdg_shell_v6_surface);
	struct wlr_xdg_surface_v6 *xdg_surface = data;

	if (xdg_surface->role == WLR_XDG_SURFACE_V6_ROLE_POPUP) {
		// TODO: popups
		return;
	}

	sway_log(L_DEBUG, "New xdg_shell_v6 toplevel title='%s' app_id='%s'",
			xdg_surface->title, xdg_surface->app_id);
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
	sway_view->iface.set_dimensions = set_dimensions;
	sway_view->wlr_xdg_surface_v6 = xdg_surface;
	sway_view->sway_xdg_surface_v6 = sway_surface;
	sway_view->surface = xdg_surface->surface;
	sway_surface->view = sway_view;
	
	// TODO:
	// - Wire up listeners
	// - Handle popups
	// - Look up pid and open on appropriate workspace
	// - Set new view to maximized so it behaves nicely
	// - Criteria

	// TODO: actual focus semantics
	swayc_t *parent = root_container.children->items[0];
	parent = parent->children->items[0]; // workspace

	swayc_t *cont = new_view(parent, sway_view);
	sway_view->swayc = cont;

	arrange_windows(cont->parent, -1, -1);
}
