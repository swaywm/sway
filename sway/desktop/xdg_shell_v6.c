#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include "sway/server.h"
#include "sway/view.h"
#include "log.h"

static const char *get_prop(struct sway_view *view, enum sway_view_prop prop) {
	if (!sway_assert(view->type == SWAY_XDG_SHELL_V6_VIEW,
				"xdg get_prop for non-xdg view!")) {
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
	sway_surface->view = sway_view;
	
	// TODO:
	// - Add to tree
	// - Wire up listeners
	// - Handle popups
	// - Look up pid and open on appropriate workspace
	// - Set new view to maximized so it behaves nicely
	// - Criteria
}
