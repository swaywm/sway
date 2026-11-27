#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/xwayland.h>
#include <xcb/xcb_icccm.h>
#include "log.h"
#include "sway/desktop/transaction.h"
#include "sway/input/cursor.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/scene_descriptor.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/server.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

static const char *atom_map[ATOM_LAST] = {
	[NET_WM_WINDOW_TYPE_NORMAL] = "_NET_WM_WINDOW_TYPE_NORMAL",
	[NET_WM_WINDOW_TYPE_DIALOG] = "_NET_WM_WINDOW_TYPE_DIALOG",
	[NET_WM_WINDOW_TYPE_UTILITY] = "_NET_WM_WINDOW_TYPE_UTILITY",
	[NET_WM_WINDOW_TYPE_TOOLBAR] = "_NET_WM_WINDOW_TYPE_TOOLBAR",
	[NET_WM_WINDOW_TYPE_SPLASH] = "_NET_WM_WINDOW_TYPE_SPLASH",
	[NET_WM_WINDOW_TYPE_MENU] = "_NET_WM_WINDOW_TYPE_MENU",
	[NET_WM_WINDOW_TYPE_DROPDOWN_MENU] = "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
	[NET_WM_WINDOW_TYPE_POPUP_MENU] = "_NET_WM_WINDOW_TYPE_POPUP_MENU",
	[NET_WM_WINDOW_TYPE_TOOLTIP] = "_NET_WM_WINDOW_TYPE_TOOLTIP",
	[NET_WM_WINDOW_TYPE_NOTIFICATION] = "_NET_WM_WINDOW_TYPE_NOTIFICATION",
	[NET_WM_STATE_MODAL] = "_NET_WM_STATE_MODAL",
	[NET_SUPPORTED] = "_NET_SUPPORTED",
	[NET_MOVERESIZE_WINDOW] = "_NET_MOVERESIZE_WINDOW",
	[NET_FRAME_EXTENTS] = "_NET_FRAME_EXTENTS",
	[NET_REQUEST_FRAME_EXTENTS] = "_NET_REQUEST_FRAME_EXTENTS",
	[NET_WM_STATE] = "_NET_WM_STATE",
	[NET_WM_STATE_STICKY] = "_NET_WM_STATE_STICKY",
	[WM_CLIENT_LEADER] = "WM_CLIENT_LEADER",
};

static void unmanaged_handle_request_configure(struct wl_listener *listener,
		void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, request_configure);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;
	struct wlr_xwayland_surface_configure_event *ev = data;
	wlr_xwayland_surface_configure(xsurface, ev->x, ev->y,
		ev->width, ev->height);
}

static void unmanaged_handle_set_geometry(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, set_geometry);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;

	wlr_scene_node_set_position(&surface->surface_scene->buffer->node, xsurface->x, xsurface->y);
}

static void unmanaged_handle_map(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, map);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;

	surface->surface_scene = wlr_scene_surface_create(root->layers.unmanaged,
		xsurface->surface);

	if (surface->surface_scene) {
		scene_descriptor_assign(&surface->surface_scene->buffer->node,
			SWAY_SCENE_DESC_XWAYLAND_UNMANAGED, surface);
		wlr_scene_node_set_position(&surface->surface_scene->buffer->node,
			xsurface->x, xsurface->y);

		wl_signal_add(&xsurface->events.set_geometry, &surface->set_geometry);
		surface->set_geometry.notify = unmanaged_handle_set_geometry;
	}

	if (wlr_xwayland_surface_override_redirect_wants_focus(xsurface)) {
		struct sway_seat *seat = input_manager_current_seat();
		struct wlr_xwayland *xwayland = server.xwayland.wlr_xwayland;
		wlr_xwayland_set_seat(xwayland, seat->wlr_seat);
		seat_set_focus_surface(seat, xsurface->surface, false);
	}
}

static void unmanaged_handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, unmap);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;

	if (surface->surface_scene) {
		wl_list_remove(&surface->set_geometry.link);

		wlr_scene_node_destroy(&surface->surface_scene->buffer->node);
		surface->surface_scene = NULL;
	}

	struct sway_seat *seat = input_manager_current_seat();
	if (seat->wlr_seat->keyboard_state.focused_surface == xsurface->surface) {
		// This simply returns focus to the parent surface if there's one available.
		// This seems to handle JetBrains issues.
		if (xsurface->parent && xsurface->parent->surface
				&& wlr_xwayland_surface_override_redirect_wants_focus(xsurface->parent)) {
			seat_set_focus_surface(seat, xsurface->parent->surface, false);
			return;
		}

		// Restore focus
		struct sway_node *previous = seat_get_focus_inactive(seat, &root->node);
		if (previous) {
			// Hack to get seat to re-focus the return value of get_focus
			seat_set_focus(seat, NULL);
			seat_set_focus(seat, previous);
		}
	}
}

static void unmanaged_handle_request_activate(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, request_activate);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
	struct sway_seat *seat = input_manager_current_seat();
	struct sway_container *focus = seat_get_focused_container(seat);
	if (focus && focus->view && focus->view->pid != xsurface->pid) {
		return;
	}

	seat_set_focus_surface(seat, xsurface->surface, false);
}

static void unmanaged_handle_associate(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, associate);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;
	wl_signal_add(&xsurface->surface->events.map, &surface->map);
	surface->map.notify = unmanaged_handle_map;
	wl_signal_add(&xsurface->surface->events.unmap, &surface->unmap);
	surface->unmap.notify = unmanaged_handle_unmap;
}

static void unmanaged_handle_dissociate(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, dissociate);
	wl_list_remove(&surface->map.link);
	wl_list_remove(&surface->unmap.link);
}

static void unmanaged_handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, destroy);
	wl_list_remove(&surface->request_configure.link);
	wl_list_remove(&surface->associate.link);
	wl_list_remove(&surface->dissociate.link);
	wl_list_remove(&surface->destroy.link);
	wl_list_remove(&surface->override_redirect.link);
	wl_list_remove(&surface->request_activate.link);
	free(surface);
}

static void handle_map(struct wl_listener *listener, void *data);
static void handle_associate(struct wl_listener *listener, void *data);

struct sway_xwayland_view *create_xwayland_view(struct wlr_xwayland_surface *xsurface);

static void unmanaged_handle_override_redirect(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, override_redirect);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;

	bool associated = xsurface->surface != NULL;
	bool mapped = associated && xsurface->surface->mapped;
	if (mapped) {
		unmanaged_handle_unmap(&surface->unmap, NULL);
	}
	if (associated) {
		unmanaged_handle_dissociate(&surface->dissociate, NULL);
	}

	unmanaged_handle_destroy(&surface->destroy, NULL);
	xsurface->data = NULL;

	struct sway_xwayland_view *xwayland_view = create_xwayland_view(xsurface);
	if (associated) {
		handle_associate(&xwayland_view->associate, NULL);
	}
	if (mapped) {
		handle_map(&xwayland_view->map, xsurface);
	}
}

static struct sway_xwayland_unmanaged *create_unmanaged(
		struct wlr_xwayland_surface *xsurface) {
	struct sway_xwayland_unmanaged *surface =
		calloc(1, sizeof(struct sway_xwayland_unmanaged));
	if (surface == NULL) {
		sway_log(SWAY_ERROR, "Allocation failed");
		return NULL;
	}

	surface->wlr_xwayland_surface = xsurface;

	wl_signal_add(&xsurface->events.request_configure,
		&surface->request_configure);
	surface->request_configure.notify = unmanaged_handle_request_configure;
	wl_signal_add(&xsurface->events.associate, &surface->associate);
	surface->associate.notify = unmanaged_handle_associate;
	wl_signal_add(&xsurface->events.dissociate, &surface->dissociate);
	surface->dissociate.notify = unmanaged_handle_dissociate;
	wl_signal_add(&xsurface->events.destroy, &surface->destroy);
	surface->destroy.notify = unmanaged_handle_destroy;
	wl_signal_add(&xsurface->events.set_override_redirect, &surface->override_redirect);
	surface->override_redirect.notify = unmanaged_handle_override_redirect;
	wl_signal_add(&xsurface->events.request_activate, &surface->request_activate);
	surface->request_activate.notify = unmanaged_handle_request_activate;

	return surface;
}

static struct sway_xwayland_view *xwayland_view_from_view(
		struct sway_view *view) {
	if (!sway_assert(view->type == SWAY_VIEW_XWAYLAND,
			"Expected xwayland view")) {
		return NULL;
	}
	return (struct sway_xwayland_view *)view;
}

static const char *get_string_prop(struct sway_view *view, enum sway_view_prop prop) {
	if (xwayland_view_from_view(view) == NULL) {
		return NULL;
	}
	switch (prop) {
	case VIEW_PROP_TITLE:
		return view->wlr_xwayland_surface->title;
	case VIEW_PROP_CLASS:
		return view->wlr_xwayland_surface->class;
	case VIEW_PROP_INSTANCE:
		return view->wlr_xwayland_surface->instance;
	case VIEW_PROP_WINDOW_ROLE:
		return view->wlr_xwayland_surface->role;
	default:
		return NULL;
	}
}

static uint32_t get_int_prop(struct sway_view *view, enum sway_view_prop prop) {
	if (xwayland_view_from_view(view) == NULL) {
		return 0;
	}
	switch (prop) {
	case VIEW_PROP_X11_WINDOW_ID:
		return view->wlr_xwayland_surface->window_id;
	case VIEW_PROP_X11_PARENT_ID:
		if (view->wlr_xwayland_surface->parent) {
			return view->wlr_xwayland_surface->parent->window_id;
		}
		return 0;
	case VIEW_PROP_WINDOW_TYPE:
		if (view->wlr_xwayland_surface->window_type_len == 0) {
			return 0;
		}
		return view->wlr_xwayland_surface->window_type[0];
	default:
		return 0;
	}
}

static uint32_t configure(struct sway_view *view, double lx, double ly, int width,
		int height) {
	struct sway_xwayland_view *xwayland_view = xwayland_view_from_view(view);
	if (xwayland_view == NULL) {
		return 0;
	}
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;

	wlr_xwayland_surface_configure(xsurface, lx, ly, width, height);

	// xwayland doesn't give us a serial for the configure
	return 0;
}

static void set_activated(struct sway_view *view, bool activated) {
	if (xwayland_view_from_view(view) == NULL) {
		return;
	}
	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;

	if (activated && surface->minimized) {
		wlr_xwayland_surface_set_minimized(surface, false);
	}

	wlr_xwayland_surface_activate(surface, activated);
}

static void set_tiled(struct sway_view *view, bool tiled) {
	if (xwayland_view_from_view(view) == NULL) {
		return;
	}
	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;
	wlr_xwayland_surface_set_maximized(surface, tiled, tiled);
}

static void set_fullscreen(struct sway_view *view, bool fullscreen) {
	if (xwayland_view_from_view(view) == NULL) {
		return;
	}
	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;
	wlr_xwayland_surface_set_fullscreen(surface, fullscreen);
}

static void set_sticky(struct sway_view *view, bool sticky) {
	if (xwayland_view_from_view(view) == NULL) {
		return;
	}
	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;
	// The XWM keeps _NET_WM_STATE in line with this flag
	if (surface->sticky != sticky) {
		wlr_xwayland_surface_set_sticky(surface, sticky);
	}
}

static xcb_connection_t *xwm_connection(void) {
	struct wlr_xwayland *wlr_xwayland = server.xwayland.wlr_xwayland;
	return wlr_xwayland ? wlr_xwayland_get_xwm_connection(wlr_xwayland) : NULL;
}

static void publish_frame_extents(xcb_window_t window,
		const uint32_t extents[static 4]) {
	xcb_connection_t *xcb_conn = xwm_connection();
	if (xcb_conn == NULL) {
		return;
	}
	xcb_change_property(xcb_conn, XCB_PROP_MODE_REPLACE, window,
		server.xwayland.atoms[NET_FRAME_EXTENTS], XCB_ATOM_CARDINAL, 32,
		4, extents);
	xcb_flush(xcb_conn);
}

static void set_frame_extents(struct sway_view *view,
		int left, int right, int top, int bottom) {
	struct sway_xwayland_view *xwayland_view = xwayland_view_from_view(view);
	if (xwayland_view == NULL || view->wlr_xwayland_surface == NULL ||
			xwm_connection() == NULL) {
		return;
	}
	uint32_t extents[4] = { left, right, top, bottom };
	if (xwayland_view->frame_extents_published &&
			memcmp(extents, xwayland_view->frame_extents, sizeof(extents)) == 0) {
		return;
	}
	memcpy(xwayland_view->frame_extents, extents, sizeof(extents));
	xwayland_view->frame_extents_published = true;
	publish_frame_extents(view->wlr_xwayland_surface->window_id, extents);
}

static void place_initial_floating(struct sway_view *view);

static bool wants_floating(struct sway_view *view) {
	if (xwayland_view_from_view(view) == NULL) {
		return false;
	}
	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;
	struct sway_xwayland *xwayland = &server.xwayland;

	if (surface->modal) {
		return true;
	}

	for (size_t i = 0; i < surface->window_type_len; ++i) {
		xcb_atom_t type = surface->window_type[i];
		if (type == xwayland->atoms[NET_WM_WINDOW_TYPE_DIALOG] ||
				type == xwayland->atoms[NET_WM_WINDOW_TYPE_UTILITY] ||
				type == xwayland->atoms[NET_WM_WINDOW_TYPE_TOOLBAR] ||
				type == xwayland->atoms[NET_WM_WINDOW_TYPE_SPLASH]) {
			return true;
		}
	}

	xcb_size_hints_t *size_hints = surface->size_hints;
	if (size_hints != NULL &&
			size_hints->min_width > 0 && size_hints->min_height > 0 &&
			(size_hints->max_width == size_hints->min_width ||
			size_hints->max_height == size_hints->min_height)) {
		return true;
	}

	return false;
}

static void handle_set_decorations(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_decorations);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;

	bool csd = xsurface->decorations != WLR_XWAYLAND_SURFACE_DECORATIONS_ALL;
	view_update_csd_from_client(view, csd);
}

static bool is_transient_for(struct sway_view *child,
		struct sway_view *ancestor) {
	if (xwayland_view_from_view(child) == NULL) {
		return false;
	}
	struct wlr_xwayland_surface *surface = child->wlr_xwayland_surface;
	while (surface) {
		if (surface->parent == ancestor->wlr_xwayland_surface) {
			return true;
		}
		surface = surface->parent;
	}
	return false;
}

static void _close(struct sway_view *view) {
	if (xwayland_view_from_view(view) == NULL) {
		return;
	}
	wlr_xwayland_surface_close(view->wlr_xwayland_surface);
}

static void destroy(struct sway_view *view) {
	struct sway_xwayland_view *xwayland_view = xwayland_view_from_view(view);
	if (xwayland_view == NULL) {
		return;
	}
	free(xwayland_view);
}

static void get_constraints(struct sway_view *view, double *min_width,
		double *max_width, double *min_height, double *max_height) {
	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;
	xcb_size_hints_t *size_hints = surface->size_hints;

	if (size_hints == NULL) {
		*min_width = DBL_MIN;
		*max_width = DBL_MAX;
		*min_height = DBL_MIN;
		*max_height = DBL_MAX;
		return;
	}

	*min_width = size_hints->min_width > 0 ? size_hints->min_width : DBL_MIN;
	*max_width = size_hints->max_width > 0 ? size_hints->max_width : DBL_MAX;
	*min_height = size_hints->min_height > 0 ? size_hints->min_height : DBL_MIN;
	*max_height = size_hints->max_height > 0 ? size_hints->max_height : DBL_MAX;
}

static const struct sway_view_impl view_impl = {
	.get_constraints = get_constraints,
	.get_string_prop = get_string_prop,
	.get_int_prop = get_int_prop,
	.configure = configure,
	.set_activated = set_activated,
	.set_tiled = set_tiled,
	.set_fullscreen = set_fullscreen,
	.set_sticky = set_sticky,
	.set_frame_extents = set_frame_extents,
	.place_initial_floating = place_initial_floating,
	.wants_floating = wants_floating,
	.is_transient_for = is_transient_for,
	.close = _close,
	.destroy = destroy,
};

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, commit);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	struct wlr_surface_state *state = &xsurface->surface->current;

	struct wlr_box new_geo = {0};
	new_geo.width = state->width;
	new_geo.height = state->height;

	bool new_size = new_geo.width != view->geometry.width ||
			new_geo.height != view->geometry.height;

	if (new_size) {
		// The client changed its surface size in this commit. For floating
		// containers, we resize the container to match. For tiling containers,
		// we only recenter the surface.
		memcpy(&view->geometry, &new_geo, sizeof(struct wlr_box));
		if (container_is_floating(view->container)) {
			view_update_size(view);
			transaction_commit_dirty_client();
		}

		view_center_and_clip_surface(view);
	}

	if (view->container->node.instruction) {
		bool successful = transaction_notify_view_ready_by_geometry(view,
				xsurface->x, xsurface->y, state->width, state->height);

		// If we saved the view and this commit isn't what we're looking for
		// that means the user will never actually see the buffers submitted to
		// us here. Just send frame done events to these surfaces so they can
		// commit another time for us.
		if (view->saved_surface_tree && !successful) {
			view_send_frame_done(view);
		}
	}
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, destroy);
	struct sway_view *view = &xwayland_view->view;

	if (view->surface) {
		view_unmap(view);
		wl_list_remove(&xwayland_view->commit.link);
	}

	xwayland_view->view.wlr_xwayland_surface = NULL;

	wl_list_remove(&xwayland_view->destroy.link);
	wl_list_remove(&xwayland_view->request_configure.link);
	wl_list_remove(&xwayland_view->request_fullscreen.link);
	wl_list_remove(&xwayland_view->request_minimize.link);
	wl_list_remove(&xwayland_view->request_move.link);
	wl_list_remove(&xwayland_view->request_resize.link);
	wl_list_remove(&xwayland_view->request_activate.link);
	wl_list_remove(&xwayland_view->request_close.link);
	wl_list_remove(&xwayland_view->request_sticky.link);
	wl_list_remove(&xwayland_view->map_request.link);
	wl_list_remove(&xwayland_view->set_title.link);
	wl_list_remove(&xwayland_view->set_class.link);
	wl_list_remove(&xwayland_view->set_role.link);
	wl_list_remove(&xwayland_view->set_startup_id.link);
	wl_list_remove(&xwayland_view->set_window_type.link);
	wl_list_remove(&xwayland_view->set_hints.link);
	wl_list_remove(&xwayland_view->set_decorations.link);
	wl_list_remove(&xwayland_view->associate.link);
	wl_list_remove(&xwayland_view->dissociate.link);
	wl_list_remove(&xwayland_view->override_redirect.link);
	view_begin_destroy(&xwayland_view->view);
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, unmap);
	struct sway_view *view = &xwayland_view->view;

	if (!sway_assert(view->surface, "Cannot unmap unmapped view")) {
		return;
	}

	wl_list_remove(&xwayland_view->commit.link);
	wl_list_remove(&xwayland_view->surface_tree_destroy.link);

	wlr_scene_node_destroy(&xwayland_view->image_capture_scene_surface->buffer->node);
	xwayland_view->image_capture_scene_surface = NULL;

	if (xwayland_view->surface_tree) {
		wlr_scene_node_destroy(&xwayland_view->surface_tree->node);
		xwayland_view->surface_tree = NULL;
	}

	view_unmap(view);
}

static void handle_surface_tree_destroy(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view = wl_container_of(listener, xwayland_view,
		surface_tree_destroy);
	xwayland_view->surface_tree = NULL;
}

static void handle_map_request(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, map_request);
	struct wlr_xwayland_surface *xsurface = xwayland_view->view.wlr_xwayland_surface;
	struct sway_xwayland *xwayland = &server.xwayland;
	xwayland_view->client_leader = XCB_WINDOW_NONE;
	xcb_connection_t *xcb_conn = xwm_connection();
	if (xcb_conn == NULL) {
		return;
	}

	// The XWM doesn't track WM_CLIENT_LEADER. Like it does for the other
	// properties, read it while handling the MapRequest: events queued
	// meanwhile are handled after this one.
	xcb_get_property_cookie_t cookie = xcb_get_property(xcb_conn, 0,
		xsurface->window_id, xwayland->atoms[WM_CLIENT_LEADER],
		XCB_ATOM_WINDOW, 0, 1);
	xcb_get_property_reply_t *reply =
		xcb_get_property_reply(xcb_conn, cookie, NULL);
	if (reply == NULL) {
		return;
	}
	if (reply->type == XCB_ATOM_WINDOW && reply->format == 32 &&
			xcb_get_property_value_length(reply) >= (int)sizeof(xcb_window_t)) {
		xwayland_view->client_leader =
			*(xcb_window_t *)xcb_get_property_value(reply);
	}
	free(reply);
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, map);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;

	view->natural_width = xsurface->width;
	view->natural_height = xsurface->height;

	// Wire up the commit listener here, because xwayland map/unmap can change
	// the underlying wlr_surface
	wl_signal_add(&xsurface->surface->events.commit, &xwayland_view->commit);
	xwayland_view->commit.notify = handle_commit;

	// The window may carry extents estimated before it was mapped, or left
	// over from a previous mapping
	xwayland_view->frame_extents_published = false;

	// Put it back into the tree
	view_map(view, xsurface->surface, xsurface->fullscreen, NULL, false);

	// The client's initial _NET_WM_STATE_STICKY isn't honoured; reflect
	// the container's actual state
	view_update_sticky(view);

	xwayland_view->surface_tree = wlr_scene_subsurface_tree_create(
		xwayland_view->view.content_tree, xsurface->surface);

	if (xwayland_view->surface_tree) {
		xwayland_view->surface_tree_destroy.notify = handle_surface_tree_destroy;
		wl_signal_add(&xwayland_view->surface_tree->node.events.destroy,
			&xwayland_view->surface_tree_destroy);
	}

	xwayland_view->image_capture_scene_surface =
		wlr_scene_surface_create(&xwayland_view->view.image_capture_scene->tree, xsurface->surface);

	transaction_commit_dirty();
}

static void handle_dissociate(struct wl_listener *listener, void *data);

static void handle_override_redirect(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, override_redirect);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;

	bool associated = xsurface->surface != NULL;
	bool mapped = associated && xsurface->surface->mapped;
	if (mapped) {
		handle_unmap(&xwayland_view->unmap, NULL);
	}
	if (associated) {
		handle_dissociate(&xwayland_view->dissociate, NULL);
	}

	handle_destroy(&xwayland_view->destroy, view);
	xsurface->data = NULL;

	struct sway_xwayland_unmanaged *unmanaged = create_unmanaged(xsurface);
	if (associated) {
		unmanaged_handle_associate(&unmanaged->associate, NULL);
	}
	if (mapped) {
		unmanaged_handle_map(&unmanaged->map, xsurface);
	}
}

static void send_current_geometry(struct sway_view *view) {
	struct sway_container *con = view->container;
	configure(view, con->current.content_x, con->current.content_y,
		con->current.content_width, con->current.content_height);
}

/**
 * Whether another window is fullscreen on the container's workspace. A window
 * hidden in the scratchpad has no workspace.
 */
static bool fullscreen_sibling(struct sway_container *con) {
	struct sway_workspace *workspace = con->pending.workspace;
	if (workspace == NULL) {
		return false;
	}
	if (workspace->fullscreen && workspace->fullscreen != con) {
		return true;
	}
	struct sway_container *global = root->fullscreen_global;
	return global && global != con && global->pending.workspace == workspace;
}

/**
 * Handles a client's request to move or resize its window. The mask selects
 * the requested fields as XCB_CONFIG_WINDOW_{X,Y,WIDTH,HEIGHT} flags; x and y
 * are the requested position of the client window itself.
 */
static void request_geometry(struct sway_view *view, uint16_t mask,
		int x, int y, uint32_t width, uint32_t height) {
	struct sway_container *con = view->container;
	if (!container_is_floating(con) ||
			con->pending.fullscreen_mode != FULLSCREEN_NONE ||
			fullscreen_sibling(con)) {
		// Tiled and fullscreen windows keep their geometry, and so does a
		// floating window while another one is fullscreen on its workspace
		send_current_geometry(view);
		return;
	}

	// A window hidden in the scratchpad stays hidden: it takes the
	// requested size, but any requested position is discarded
	if (container_is_scratchpad_hidden(con)) {
		mask &= ~(XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y);
	}
	if (!(mask & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
			XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT))) {
		send_current_geometry(view);
		return;
	}

	double content_x = mask & XCB_CONFIG_WINDOW_X ? x : con->pending.content_x;
	double content_y = mask & XCB_CONFIG_WINDOW_Y ? y : con->pending.content_y;

	// Only a requested position can move the window to another workspace:
	// the visible one of the output it lies on, if that's another output. A
	// requested position that lies on no output voids the whole request.
	struct sway_workspace *workspace = con->pending.workspace;
	struct sway_output *output = workspace ? workspace->output : NULL;
	if (mask & (XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y)) {
		struct wlr_output *wlr_output = wlr_output_layout_output_at(
				root->output_layout, content_x, content_y);
		struct sway_output *target = wlr_output ? wlr_output->data : NULL;
		if (target == NULL) {
			send_current_geometry(view);
			return;
		}
		if (target != output) {
			workspace = output_get_active_workspace(target);
			output = target;
			if (workspace == NULL) {
				send_current_geometry(view);
				return;
			}
		}
	}

	// Respect minimum and maximum sizes
	int min_width, max_width, min_height, max_height;
	floating_calculate_constraints(&min_width, &max_width,
			&min_height, &max_height);
	if (mask & XCB_CONFIG_WINDOW_WIDTH) {
		con->pending.content_width =
			fmax(min_width, fmin((double)width, max_width));
		view->natural_width = con->pending.content_width;
	}
	if (mask & XCB_CONFIG_WINDOW_HEIGHT) {
		con->pending.content_height =
			fmax(min_height, fmin((double)height, max_height));
		view->natural_height = con->pending.content_height;
	}
	con->pending.content_x = content_x;
	con->pending.content_y = content_y;
	container_set_geometry_from_content(con);

	struct sway_workspace *old_workspace = con->pending.workspace;
	if (old_workspace != NULL && workspace != old_workspace) {
		container_detach(con);
		workspace_add_floating(workspace, con);
		arrange_workspace(old_workspace);
		arrange_workspace(workspace);
		// If the moved container was a visible scratchpad container, then
		// update its transform.
		if (con->scratchpad) {
			struct wlr_box output_box;
			output_get_box(output, &output_box);
			con->transform = output_box;
		}
		workspace_detect_urgent(old_workspace);
		workspace_detect_urgent(workspace);
		ipc_event_window(con, "move");
		workspace_consider_destroy(old_workspace);
		seat_set_focus_container(input_manager_current_seat(), con);
	} else {
		arrange_container(con);
	}

	// The X server only notifies the client of actual changes
	if ((int)con->pending.content_x == (int)con->current.content_x &&
			(int)con->pending.content_y == (int)con->current.content_y &&
			(int)con->pending.content_width == (int)con->current.content_width &&
			(int)con->pending.content_height == (int)con->current.content_height) {
		send_current_geometry(view);
	}

	transaction_commit_dirty();
}

static void handle_request_configure(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_configure);
	struct wlr_xwayland_surface_configure_event *ev = data;
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		wlr_xwayland_surface_configure(xsurface, ev->x, ev->y,
			ev->width, ev->height);
		return;
	}
	request_geometry(view, ev->mask, ev->x, ev->y, ev->width, ev->height);
}

static void handle_request_fullscreen(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_fullscreen);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
	container_set_fullscreen(view->container, xsurface->fullscreen);

	arrange_root();
	transaction_commit_dirty();
}

static void handle_request_minimize(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_minimize);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}

	struct wlr_xwayland_minimize_event *e = data;
	struct sway_seat *seat = input_manager_current_seat();
	bool focused = seat_get_focus(seat) == &view->container->node;
	wlr_xwayland_surface_set_minimized(xsurface, !focused && e->minimize);
}

static void handle_request_move(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_move);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
	if (!container_is_floating(view->container) ||
			view->container->pending.fullscreen_mode) {
		return;
	}
	struct sway_seat *seat = input_manager_current_seat();
	seatop_begin_move_floating(seat, view->container);
}

static void handle_request_resize(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_resize);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
	if (!container_is_floating(view->container)) {
		return;
	}
	struct wlr_xwayland_resize_event *e = data;
	struct sway_seat *seat = input_manager_current_seat();
	seatop_begin_resize_floating(seat, view->container, e->edges);
}

static void handle_request_activate(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_activate);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
	view_request_activate(view, NULL);

	transaction_commit_dirty();
}

static void handle_request_close(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_close);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
	view_close(view);
}

static void handle_request_sticky(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_sticky);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
	// handle_xwayland_event() has already applied the request to the
	// container before the XWM applied it to its own sticky flag, which
	// mirrors whether the view counts as sticky. Only restore that flag.
	view_update_sticky(view);
}

static void handle_set_title(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_title);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
	view_update_title(view, false);
	view_execute_criteria(view);
	transaction_commit_dirty();
}

static void handle_set_class(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_class);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
	view_execute_criteria(view);
	transaction_commit_dirty();
}

static void handle_set_role(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_role);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
	view_execute_criteria(view);
	transaction_commit_dirty();
}

static void handle_set_startup_id(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_startup_id);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->startup_id == NULL) {
		return;
	}

	struct wlr_xdg_activation_token_v1 *token =
		wlr_xdg_activation_v1_find_token(
				server.xdg_activation_v1, xsurface->startup_id);
	if (token == NULL) {
		// Tried to activate with an unknown or expired token
		return;
	}

	struct launcher_ctx *ctx = token->data;
	if (token->data == NULL) {
		// TODO: support external launchers in X
		return;
	}
	view_assign_ctx(view, ctx);
}

static void handle_set_window_type(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_window_type);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
	view_execute_criteria(view);
	transaction_commit_dirty();
}

static void handle_set_hints(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_hints);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->surface == NULL || !xsurface->surface->mapped) {
		return;
	}
	const bool hints_urgency = xcb_icccm_wm_hints_get_urgency(xsurface->hints);
	if (!hints_urgency && view->urgent_timer) {
		// The view is in the timeout period. We'll ignore the request to
		// unset urgency so that the view remains urgent until the timer clears
		// it.
		return;
	}
	if (view->allow_request_urgent) {
		view_set_urgent(view, hints_urgency);
	}
}

static void handle_associate(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, associate);
	struct wlr_xwayland_surface *xsurface =
		xwayland_view->view.wlr_xwayland_surface;
	wl_signal_add(&xsurface->surface->events.unmap, &xwayland_view->unmap);
	xwayland_view->unmap.notify = handle_unmap;
	wl_signal_add(&xsurface->surface->events.map, &xwayland_view->map);
	xwayland_view->map.notify = handle_map;
}

static void handle_dissociate(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, dissociate);
	wl_list_remove(&xwayland_view->map.link);
	wl_list_remove(&xwayland_view->unmap.link);
}

struct sway_view *view_from_wlr_xwayland_surface(
		struct wlr_xwayland_surface *xsurface) {
	return xsurface->data;
}

struct sway_xwayland_view *create_xwayland_view(struct wlr_xwayland_surface *xsurface) {
	sway_log(SWAY_DEBUG, "New xwayland surface title='%s' class='%s'",
		xsurface->title, xsurface->class);

	struct sway_xwayland_view *xwayland_view =
		calloc(1, sizeof(struct sway_xwayland_view));
	if (!sway_assert(xwayland_view, "Failed to allocate view")) {
		return NULL;
	}

	if (!view_init(&xwayland_view->view, SWAY_VIEW_XWAYLAND, &view_impl)) {
		free(xwayland_view);
		return NULL;
	}
	xwayland_view->view.wlr_xwayland_surface = xsurface;

	wl_signal_add(&xsurface->events.destroy, &xwayland_view->destroy);
	xwayland_view->destroy.notify = handle_destroy;

	wl_signal_add(&xsurface->events.request_configure,
		&xwayland_view->request_configure);
	xwayland_view->request_configure.notify = handle_request_configure;

	wl_signal_add(&xsurface->events.request_fullscreen,
		&xwayland_view->request_fullscreen);
	xwayland_view->request_fullscreen.notify = handle_request_fullscreen;

	wl_signal_add(&xsurface->events.request_minimize,
		&xwayland_view->request_minimize);
	xwayland_view->request_minimize.notify = handle_request_minimize;

	wl_signal_add(&xsurface->events.request_activate,
		&xwayland_view->request_activate);
	xwayland_view->request_activate.notify = handle_request_activate;

	wl_signal_add(&xsurface->events.request_close,
		&xwayland_view->request_close);
	xwayland_view->request_close.notify = handle_request_close;

	wl_signal_add(&xsurface->events.request_sticky,
		&xwayland_view->request_sticky);
	xwayland_view->request_sticky.notify = handle_request_sticky;

	wl_signal_add(&xsurface->events.map_request, &xwayland_view->map_request);
	xwayland_view->map_request.notify = handle_map_request;

	wl_signal_add(&xsurface->events.request_move,
		&xwayland_view->request_move);
	xwayland_view->request_move.notify = handle_request_move;

	wl_signal_add(&xsurface->events.request_resize,
		&xwayland_view->request_resize);
	xwayland_view->request_resize.notify = handle_request_resize;

	wl_signal_add(&xsurface->events.set_title, &xwayland_view->set_title);
	xwayland_view->set_title.notify = handle_set_title;

	wl_signal_add(&xsurface->events.set_class, &xwayland_view->set_class);
	xwayland_view->set_class.notify = handle_set_class;

	wl_signal_add(&xsurface->events.set_role, &xwayland_view->set_role);
	xwayland_view->set_role.notify = handle_set_role;

	wl_signal_add(&xsurface->events.set_startup_id,
			&xwayland_view->set_startup_id);
	xwayland_view->set_startup_id.notify = handle_set_startup_id;

	wl_signal_add(&xsurface->events.set_window_type,
			&xwayland_view->set_window_type);
	xwayland_view->set_window_type.notify = handle_set_window_type;

	wl_signal_add(&xsurface->events.set_hints, &xwayland_view->set_hints);
	xwayland_view->set_hints.notify = handle_set_hints;

	wl_signal_add(&xsurface->events.set_decorations,
			&xwayland_view->set_decorations);
	xwayland_view->set_decorations.notify = handle_set_decorations;

	wl_signal_add(&xsurface->events.associate, &xwayland_view->associate);
	xwayland_view->associate.notify = handle_associate;

	wl_signal_add(&xsurface->events.dissociate, &xwayland_view->dissociate);
	xwayland_view->dissociate.notify = handle_dissociate;

	wl_signal_add(&xsurface->events.set_override_redirect,
			&xwayland_view->override_redirect);
	xwayland_view->override_redirect.notify = handle_override_redirect;

	xsurface->data = xwayland_view;

	return xwayland_view;
}

void handle_xwayland_surface(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_surface *xsurface = data;

	if (xsurface->override_redirect) {
		sway_log(SWAY_DEBUG, "New xwayland unmanaged surface");
		create_unmanaged(xsurface);
		return;
	}

	create_xwayland_view(xsurface);
}

static bool find_by_x11_window(struct sway_container *con, void *data) {
	xcb_window_t *window = data;
	return con->view && con->view->type == SWAY_VIEW_XWAYLAND &&
		con->view->surface && con->view->wlr_xwayland_surface &&
		con->view->wlr_xwayland_surface->window_id == *window;
}

static struct sway_view *view_from_x11_window(xcb_window_t window) {
	struct sway_container *con = root_find_container(find_by_x11_window, &window);
	return con ? con->view : NULL;
}

/**
 * Returns the managed view of the window that the view's WM_CLIENT_LEADER
 * property named when it asked to be mapped, unless that's its own window.
 */
static struct sway_view *client_leader_view(struct sway_view *view) {
	struct sway_xwayland_view *xwayland_view = xwayland_view_from_view(view);
	xcb_window_t leader = xwayland_view->client_leader;
	if (leader == XCB_WINDOW_NONE ||
			leader == view->wlr_xwayland_surface->window_id) {
		return NULL;
	}
	return view_from_x11_window(leader);
}

/**
 * Moves the floating container so that its frame, title bar included, is
 * centred in the box, like container_floating_move_to_center() does on the
 * workspace.
 */
static void center_in_box(struct sway_container *con, const struct wlr_box *box) {
	double lx = box->x + (box->width - con->pending.width) / 2;
	double ly = box->y + (box->height - con->pending.height) / 2;
	container_floating_translate(con, lx - con->pending.x, ly - con->pending.y);
}

/**
 * Like i3, a window that is floating when it's managed appears with its own
 * geometry, or the position and size of its WM_NORMAL_HINTS, rather than
 * centred on the workspace. It stays on the workspace it maps on.
 */
static void place_initial_floating(struct sway_view *view) {
	struct sway_container *con = view->container;
	struct sway_workspace *workspace = con->pending.workspace;
	if (xwayland_view_from_view(view) == NULL || workspace == NULL ||
			workspace->output == NULL ||
			con->pending.fullscreen_mode != FULLSCREEN_NONE) {
		return;
	}
	struct wlr_output *workspace_output = workspace->output->wlr_output;
	struct wlr_box output_box;
	wlr_output_layout_get_box(root->output_layout, workspace_output, &output_box);
	if (wlr_box_empty(&output_box)) {
		return;
	}

	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	int x = xsurface->x, y = xsurface->y;
	int width = xsurface->width, height = xsurface->height;
	xcb_size_hints_t *hints = xsurface->size_hints;
	if (hints != NULL && (hints->flags & (XCB_ICCCM_SIZE_HINT_US_POSITION |
				XCB_ICCCM_SIZE_HINT_P_POSITION)) &&
			(hints->flags & (XCB_ICCCM_SIZE_HINT_US_SIZE |
				XCB_ICCCM_SIZE_HINT_P_SIZE))) {
		x = hints->x;
		y = hints->y;
		width = hints->width;
		height = hints->height;
	}

	// Respect minimum and maximum sizes
	int min_width, max_width, min_height, max_height;
	floating_calculate_constraints(&min_width, &max_width,
			&min_height, &max_height);
	con->pending.content_width = fmax(min_width, fmin(width, max_width));
	con->pending.content_height = fmax(min_height, fmin(height, max_height));
	view->natural_width = con->pending.content_width;
	view->natural_height = con->pending.content_height;
	con->pending.content_x = x;
	con->pending.content_y = y;
	container_set_geometry_from_content(con);

	if (x == 0 && y == 0) {
		// The window hasn't picked a position: centre it over its leader
		// window, or else on the workspace
		struct sway_view *leader = client_leader_view(view);
		if (leader != NULL && leader->container != NULL) {
			struct wlr_box box;
			container_get_box(leader->container, &box);
			center_in_box(con, &box);
		} else {
			container_floating_move_to_center(con);
		}
		return;
	}

	// A position on another output is carried over to the workspace's
	// output, keeping the relative position of the container's midpoint, as
	// when a floating container moves to another output. A position on no
	// output is replaced by the centre of the workspace.
	struct wlr_output *wlr_output =
		wlr_output_layout_output_at(root->output_layout, x, y);
	if (wlr_output == NULL) {
		container_floating_move_to_center(con);
	} else if (wlr_output != workspace_output) {
		struct wlr_box box;
		wlr_output_layout_get_box(root->output_layout, wlr_output, &box);
		double old_x = con->pending.x, old_y = con->pending.y;
		floating_fix_coordinates(con, &box, &output_box);
		con->pending.content_x += con->pending.x - old_x;
		con->pending.content_y += con->pending.y - old_y;
	}
}

static void handle_net_moveresize_window(xcb_client_message_event_t *ev) {
	struct sway_view *view = view_from_x11_window(ev->window);
	if (view == NULL) {
		return;
	}
	// Bits 8 to 11 select x, y, width and height, in the same order as the
	// XCB_CONFIG_WINDOW_* flags. The gravity and source indication bits don't
	// change the outcome.
	uint32_t flags = ev->data.data32[0];
	uint16_t mask = (flags >> 8) & 0xf;
	request_geometry(view, mask, (int32_t)ev->data.data32[1],
		(int32_t)ev->data.data32[2], ev->data.data32[3], ev->data.data32[4]);
}

static void handle_net_request_frame_extents(xcb_client_message_event_t *ev) {
	struct sway_view *view = view_from_x11_window(ev->window);
	if (view != NULL) {
		struct sway_xwayland_view *xwayland_view = xwayland_view_from_view(view);
		if (xwayland_view->frame_extents_published) {
			publish_frame_extents(ev->window, xwayland_view->frame_extents);
		}
		return;
	}

	// The window isn't mapped yet. Like i3, estimate the extents from the
	// default border of a tiled window.
	uint32_t border = config->border == B_NONE ? 0 : config->border_thickness;
	uint32_t extents[4] = {
		border, border, container_titlebar_height(), border,
	};
	publish_frame_extents(ev->window, extents);
}

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2

/**
 * Applies _NET_WM_STATE_STICKY requests to the container's own sticky flag,
 * like the sticky command.
 */
static void handle_net_wm_state(struct sway_xwayland *xwayland,
		xcb_client_message_event_t *ev) {
	struct sway_view *view = view_from_x11_window(ev->window);
	if (view == NULL) {
		return;
	}
	struct sway_container *con = view->container;
	uint32_t action = ev->data.data32[0];
	bool requested = false;
	bool sticky = con->is_sticky;
	// data32[1] and data32[2] name the properties to change
	for (size_t i = 1; i <= 2; ++i) {
		if (ev->data.data32[i] != xwayland->atoms[NET_WM_STATE_STICKY]) {
			continue;
		}
		switch (action) {
		case _NET_WM_STATE_REMOVE:
			sticky = false;
			break;
		case _NET_WM_STATE_ADD:
			sticky = true;
			break;
		case _NET_WM_STATE_TOGGLE:
			sticky = !sticky;
			break;
		default:
			continue;
		}
		requested = true;
	}
	if (!requested) {
		return;
	}

	container_set_sticky(con, sticky);

	transaction_commit_dirty();
}

bool handle_xwayland_event(struct wlr_xwayland *wlr_xwayland,
		xcb_generic_event_t *event) {
	struct sway_xwayland *xwayland = &server.xwayland;
	if ((event->response_type & ~0x80) != XCB_CLIENT_MESSAGE) {
		return false;
	}
	xcb_client_message_event_t *ev = (xcb_client_message_event_t *)event;
	if (ev->format != 32) {
		return false;
	}
	if (ev->type == xwayland->atoms[NET_MOVERESIZE_WINDOW]) {
		handle_net_moveresize_window(ev);
		return true;
	} else if (ev->type == xwayland->atoms[NET_REQUEST_FRAME_EXTENTS]) {
		handle_net_request_frame_extents(ev);
		return true;
	} else if (ev->type == xwayland->atoms[NET_WM_STATE]) {
		// The XWM still handles the other states of the message
		handle_net_wm_state(xwayland, ev);
	}
	return false;
}

void handle_xwayland_ready(struct wl_listener *listener, void *data) {
	struct sway_server *server =
		wl_container_of(listener, server, xwayland_ready);
	struct sway_xwayland *xwayland = &server->xwayland;

	xcb_connection_t *xcb_conn = xcb_connect(NULL, NULL);
	int err = xcb_connection_has_error(xcb_conn);
	if (err) {
		sway_log(SWAY_ERROR, "XCB connect failed: %d", err);
		return;
	}

	xcb_intern_atom_cookie_t cookies[ATOM_LAST];
	for (size_t i = 0; i < ATOM_LAST; i++) {
		cookies[i] =
			xcb_intern_atom(xcb_conn, 0, strlen(atom_map[i]), atom_map[i]);
	}
	for (size_t i = 0; i < ATOM_LAST; i++) {
		xcb_generic_error_t *error = NULL;
		xcb_intern_atom_reply_t *reply =
			xcb_intern_atom_reply(xcb_conn, cookies[i], &error);
		if (reply != NULL && error == NULL) {
			xwayland->atoms[i] = reply->atom;
		}
		free(reply);

		if (error != NULL) {
			sway_log(SWAY_ERROR, "could not resolve atom %s, X11 error code %d",
				atom_map[i], error->error_code);
			free(error);
			break;
		}
	}

	xcb_disconnect(xcb_conn);

	// Extend the XWM's _NET_SUPPORTED list. Going through its connection
	// orders this after the XWM's own list.
	xcb_connection_t *xwm_conn =
		wlr_xwayland_get_xwm_connection(xwayland->wlr_xwayland);
	if (xwm_conn != NULL) {
		xcb_screen_t *screen =
			xcb_setup_roots_iterator(xcb_get_setup(xwm_conn)).data;
		xcb_change_property(xwm_conn, XCB_PROP_MODE_APPEND, screen->root,
			xwayland->atoms[NET_SUPPORTED], XCB_ATOM_ATOM, 32, 1,
			&xwayland->atoms[NET_MOVERESIZE_WINDOW]);
		xcb_flush(xwm_conn);
	}
}
