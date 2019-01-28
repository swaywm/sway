#define _POSIX_C_SOURCE 199309L
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/xwayland.h>
#include "log.h"
#include "sway/desktop.h"
#include "sway/desktop/transaction.h"
#include "sway/input/cursor.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

static const char *atom_map[ATOM_LAST] = {
	"_NET_WM_WINDOW_TYPE_NORMAL",
	"_NET_WM_WINDOW_TYPE_DIALOG",
	"_NET_WM_WINDOW_TYPE_UTILITY",
	"_NET_WM_WINDOW_TYPE_TOOLBAR",
	"_NET_WM_WINDOW_TYPE_SPLASH",
	"_NET_WM_WINDOW_TYPE_MENU",
	"_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
	"_NET_WM_WINDOW_TYPE_POPUP_MENU",
	"_NET_WM_WINDOW_TYPE_TOOLTIP",
	"_NET_WM_WINDOW_TYPE_NOTIFICATION",
	"_NET_WM_STATE_MODAL",
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

static void unmanaged_handle_commit(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, commit);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;

	if (xsurface->x != surface->lx || xsurface->y != surface->ly) {
		// Surface has moved
		desktop_damage_surface(xsurface->surface, surface->lx, surface->ly,
			true);
		surface->lx = xsurface->x;
		surface->ly = xsurface->y;
		desktop_damage_surface(xsurface->surface, surface->lx, surface->ly,
			true);
	} else {
		desktop_damage_surface(xsurface->surface, xsurface->x, xsurface->y,
			false);
	}
}

static void unmanaged_handle_map(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, map);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;

	wl_list_insert(root->xwayland_unmanaged.prev, &surface->link);

	wl_signal_add(&xsurface->surface->events.commit, &surface->commit);
	surface->commit.notify = unmanaged_handle_commit;

	surface->lx = xsurface->x;
	surface->ly = xsurface->y;
	desktop_damage_surface(xsurface->surface, surface->lx, surface->ly, true);

	if (wlr_xwayland_or_surface_wants_focus(xsurface)) {
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
	desktop_damage_surface(xsurface->surface, xsurface->x, xsurface->y, true);
	wl_list_remove(&surface->link);
	wl_list_remove(&surface->commit.link);

	struct sway_seat *seat = input_manager_current_seat();
	if (seat->wlr_seat->keyboard_state.focused_surface ==
			xsurface->surface) {
		// Restore focus
		struct sway_node *previous = seat_get_focus_inactive(seat, &root->node);
		if (previous) {
			// Hack to get seat to re-focus the return value of get_focus
			seat_set_focus(seat, NULL);
			seat_set_focus(seat, previous);
		}
	}
}

static void unmanaged_handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, destroy);
	wl_list_remove(&surface->map.link);
	wl_list_remove(&surface->unmap.link);
	wl_list_remove(&surface->destroy.link);
	free(surface);
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
	wl_signal_add(&xsurface->events.map, &surface->map);
	surface->map.notify = unmanaged_handle_map;
	wl_signal_add(&xsurface->events.unmap, &surface->unmap);
	surface->unmap.notify = unmanaged_handle_unmap;
	wl_signal_add(&xsurface->events.destroy, &surface->destroy);
	surface->destroy.notify = unmanaged_handle_destroy;

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
		return *view->wlr_xwayland_surface->window_type;
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
	wlr_xwayland_surface_activate(surface, activated);
}

static void set_tiled(struct sway_view *view, bool tiled) {
	if (xwayland_view_from_view(view) == NULL) {
		return;
	}
	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;
	wlr_xwayland_surface_set_maximized(surface, tiled);
}

static void set_fullscreen(struct sway_view *view, bool fullscreen) {
	if (xwayland_view_from_view(view) == NULL) {
		return;
	}
	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;
	wlr_xwayland_surface_set_fullscreen(surface, fullscreen);
}

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

	struct wlr_xwayland_surface_size_hints *size_hints = surface->size_hints;
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

static const struct sway_view_impl view_impl = {
	.get_string_prop = get_string_prop,
	.get_int_prop = get_int_prop,
	.configure = configure,
	.set_activated = set_activated,
	.set_tiled = set_tiled,
	.set_fullscreen = set_fullscreen,
	.wants_floating = wants_floating,
	.is_transient_for = is_transient_for,
	.close = _close,
	.destroy = destroy,
};

static void get_geometry(struct sway_view *view, struct wlr_box *box) {
	box->x = box->y = 0;
	if (view->surface) {
		box->width = view->surface->current.width;
		box->height = view->surface->current.height;
	} else {
		box->width = 0;
		box->height = 0;
	}
}

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, commit);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	struct wlr_surface_state *state = &xsurface->surface->current;

	if (view->container->node.instruction) {
		get_geometry(view, &view->geometry);
		transaction_notify_view_ready_by_size(view,
				state->width, state->height);
	} else {
		struct wlr_box new_geo;
		get_geometry(view, &new_geo);
		struct sway_container *con = view->container;

		if ((new_geo.width != con->surface_width ||
					new_geo.height != con->surface_height)) {
			// The view has unexpectedly sent a new size
			// eg. The Firefox "Save As" dialog when downloading a file
			desktop_damage_view(view);
			view_update_size(view, new_geo.width, new_geo.height);
			memcpy(&view->geometry, &new_geo, sizeof(struct wlr_box));
			desktop_damage_view(view);
			transaction_commit_dirty();
		} else {
			memcpy(&view->geometry, &new_geo, sizeof(struct wlr_box));
		}
	}

	view_damage_from(view);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, destroy);
	struct sway_view *view = &xwayland_view->view;

	if (view->surface) {
		view_unmap(view);
		wl_list_remove(&xwayland_view->commit.link);
	}

	wl_list_remove(&xwayland_view->destroy.link);
	wl_list_remove(&xwayland_view->request_configure.link);
	wl_list_remove(&xwayland_view->request_fullscreen.link);
	wl_list_remove(&xwayland_view->request_move.link);
	wl_list_remove(&xwayland_view->request_resize.link);
	wl_list_remove(&xwayland_view->request_activate.link);
	wl_list_remove(&xwayland_view->set_title.link);
	wl_list_remove(&xwayland_view->set_class.link);
	wl_list_remove(&xwayland_view->set_role.link);
	wl_list_remove(&xwayland_view->set_window_type.link);
	wl_list_remove(&xwayland_view->set_hints.link);
	wl_list_remove(&xwayland_view->set_decorations.link);
	wl_list_remove(&xwayland_view->map.link);
	wl_list_remove(&xwayland_view->unmap.link);
	view_begin_destroy(&xwayland_view->view);
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, unmap);
	struct sway_view *view = &xwayland_view->view;

	if (!sway_assert(view->surface, "Cannot unmap unmapped view")) {
		return;
	}

	view_unmap(view);

	wl_list_remove(&xwayland_view->commit.link);
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, map);
	struct wlr_xwayland_surface *xsurface = data;
	struct sway_view *view = &xwayland_view->view;

	if (xsurface->override_redirect) {
		// This window used not to have the override redirect flag and has it
		// now. Switch to unmanaged.
		handle_destroy(&xwayland_view->destroy, view);
		struct sway_xwayland_unmanaged *unmanaged = create_unmanaged(xsurface);
		unmanaged_handle_map(&unmanaged->map, xsurface);
		return;
	}

	view->natural_width = xsurface->width;
	view->natural_height = xsurface->height;

	// Wire up the commit listener here, because xwayland map/unmap can change
	// the underlying wlr_surface
	wl_signal_add(&xsurface->surface->events.commit, &xwayland_view->commit);
	xwayland_view->commit.notify = handle_commit;

	// Put it back into the tree
	view_map(view, xsurface->surface, xsurface->fullscreen, false);

	transaction_commit_dirty();
}

static void handle_request_configure(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_configure);
	struct wlr_xwayland_surface_configure_event *ev = data;
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (!xsurface->mapped) {
		wlr_xwayland_surface_configure(xsurface, ev->x, ev->y,
			ev->width, ev->height);
		return;
	}
	if (container_is_floating(view->container)) {
		configure(view, view->container->current.content_x,
				view->container->current.content_y, ev->width, ev->height);
	} else {
		configure(view, view->container->current.content_x,
				view->container->current.content_y,
				view->container->current.content_width,
				view->container->current.content_height);
	}
}

static void handle_request_fullscreen(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_fullscreen);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (!xsurface->mapped) {
		return;
	}
	container_set_fullscreen(view->container, xsurface->fullscreen);

	arrange_root();
	transaction_commit_dirty();
}

static void handle_request_move(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_move);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (!xsurface->mapped) {
		return;
	}
	if (!container_is_floating(view->container)) {
		return;
	}
	struct sway_seat *seat = input_manager_current_seat();
	seatop_begin_move_floating(seat, view->container, seat->last_button);
}

static void handle_request_resize(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_resize);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (!xsurface->mapped) {
		return;
	}
	if (!container_is_floating(view->container)) {
		return;
	}
	struct wlr_xwayland_resize_event *e = data;
	struct sway_seat *seat = input_manager_current_seat();
	seatop_begin_resize_floating(seat, view->container,
			seat->last_button, e->edges);
}

static void handle_request_activate(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_activate);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (!xsurface->mapped) {
		return;
	}
	view_request_activate(view);

	transaction_commit_dirty();
}

static void handle_set_title(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_title);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (!xsurface->mapped) {
		return;
	}
	view_update_title(view, false);
	view_execute_criteria(view);
}

static void handle_set_class(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_class);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (!xsurface->mapped) {
		return;
	}
	view_execute_criteria(view);
}

static void handle_set_role(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_role);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (!xsurface->mapped) {
		return;
	}
	view_execute_criteria(view);
}

static void handle_set_window_type(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_window_type);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (!xsurface->mapped) {
		return;
	}
	view_execute_criteria(view);
}

static void handle_set_hints(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, set_hints);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (!xsurface->mapped) {
		return;
	}
	if (!xsurface->hints_urgency && view->urgent_timer) {
		// The view is is in the timeout period. We'll ignore the request to
		// unset urgency so that the view remains urgent until the timer clears
		// it.
		return;
	}
	if (view->allow_request_urgent) {
		view_set_urgent(view, (bool)xsurface->hints_urgency);
	}
}

struct sway_view *view_from_wlr_xwayland_surface(
		struct wlr_xwayland_surface *xsurface) {
	return xsurface->data;
}

void handle_xwayland_surface(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_surface *xsurface = data;

	if (xsurface->override_redirect) {
		sway_log(SWAY_DEBUG, "New xwayland unmanaged surface");
		create_unmanaged(xsurface);
		return;
	}

	sway_log(SWAY_DEBUG, "New xwayland surface title='%s' class='%s'",
		xsurface->title, xsurface->class);

	struct sway_xwayland_view *xwayland_view =
		calloc(1, sizeof(struct sway_xwayland_view));
	if (!sway_assert(xwayland_view, "Failed to allocate view")) {
		return;
	}

	view_init(&xwayland_view->view, SWAY_VIEW_XWAYLAND, &view_impl);
	xwayland_view->view.wlr_xwayland_surface = xsurface;

	wl_signal_add(&xsurface->events.destroy, &xwayland_view->destroy);
	xwayland_view->destroy.notify = handle_destroy;

	wl_signal_add(&xsurface->events.request_configure,
		&xwayland_view->request_configure);
	xwayland_view->request_configure.notify = handle_request_configure;

	wl_signal_add(&xsurface->events.request_fullscreen,
		&xwayland_view->request_fullscreen);
	xwayland_view->request_fullscreen.notify = handle_request_fullscreen;

	wl_signal_add(&xsurface->events.request_activate,
		&xwayland_view->request_activate);
	xwayland_view->request_activate.notify = handle_request_activate;

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

	wl_signal_add(&xsurface->events.set_window_type,
			&xwayland_view->set_window_type);
	xwayland_view->set_window_type.notify = handle_set_window_type;

	wl_signal_add(&xsurface->events.set_hints, &xwayland_view->set_hints);
	xwayland_view->set_hints.notify = handle_set_hints;

	wl_signal_add(&xsurface->events.set_decorations,
			&xwayland_view->set_decorations);
	xwayland_view->set_decorations.notify = handle_set_decorations;

	wl_signal_add(&xsurface->events.unmap, &xwayland_view->unmap);
	xwayland_view->unmap.notify = handle_unmap;

	wl_signal_add(&xsurface->events.map, &xwayland_view->map);
	xwayland_view->map.notify = handle_map;

	xsurface->data = xwayland_view;
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
}
