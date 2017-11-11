#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include "sway/commands.h"
#include "sway/container.h"
#include "sway/focus.h"
#include "sway/ipc-server.h"
#include "sway/server.h"
#include "sway/view.h"
#include "log.h"

// TODO: move elsewhere
static void temp_ws_cleanup() {
	swayc_t *op, *ws;
	int i = 0, j;
	if (!root_container.children)
		return;
	while (i < root_container.children->length) {
		op = root_container.children->items[i++];
		if (!op->children)
			continue;
		j = 0;
		while (j < op->children->length) {
			ws = op->children->items[j++];
			if (ws->children->length == 0 && ws->floating->length == 0 && ws != op->focused) {
				if (destroy_workspace(ws)) {
					j--;
				}
			}
		}
	}
}

// TODO: move elsewhere
static swayc_t *move_focus_to_tiling(swayc_t *focused) {
	if (focused->is_floating) {
		if (focused->parent->children->length == 0) {
			return focused->parent;
		}
		// TODO find a better way of doing this
		// Or to focused container
		return get_focused_container(focused->parent->children->items[0]);
	}
	return focused;
}

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
	// - Consolodate common logic between shells
	// - Wire up listeners
	// - Handle popups
	// - Look up pid and open on appropriate workspace
	// - Set new view to maximized so it behaves nicely
	// - Criteria

	suspend_workspace_cleanup = true;
	//swayc_t *current_ws = swayc_active_workspace();
	swayc_t *prev_focus = get_focused_container(&root_container);
	swayc_t *focused = move_focus_to_tiling(prev_focus);

	// TODO: fix new_view
	swayc_t *view = new_view(focused, sway_view);
	ipc_event_window(view, "new");
	set_focused_container(view);

	swayc_t *output = swayc_parent_by_type(view, C_OUTPUT);
	arrange_windows(output, -1, -1);

	swayc_t *workspace = swayc_parent_by_type(focused, C_WORKSPACE);
	if (workspace && workspace->fullscreen) {
		set_focused_container(workspace->fullscreen);
	}
	suspend_workspace_cleanup = false;
	temp_ws_cleanup();
}
