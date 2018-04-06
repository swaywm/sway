#define _POSIX_C_SOURCE 199309L
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/xwayland.h>
#include "log.h"
#include "sway/desktop.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/tree/view.h"

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
		desktop_damage_whole_surface(xsurface->surface,
			surface->lx, surface->ly);
		surface->lx = xsurface->x;
		surface->ly = xsurface->y;
		desktop_damage_whole_surface(xsurface->surface,
			surface->lx, surface->ly);
	} else {
		desktop_damage_from_surface(xsurface->surface,
			xsurface->x, xsurface->y);
	}
}

static void unmanaged_handle_map(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, map);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;

	wl_list_insert(&root_container.sway_root->xwayland_unmanaged,
		&surface->link);

	wl_signal_add(&xsurface->surface->events.commit, &surface->commit);
	surface->commit.notify = unmanaged_handle_commit;

	surface->lx = xsurface->x;
	surface->ly = xsurface->y;
	desktop_damage_whole_surface(xsurface->surface, surface->lx, surface->ly);

	// TODO: we don't send surface enter/leave events to xwayland unmanaged
	// surfaces, but xwayland doesn't support HiDPI anyway
}

static void unmanaged_handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, unmap);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;
	desktop_damage_whole_surface(xsurface->surface, xsurface->x, xsurface->y);
	wl_list_remove(&surface->link);
	wl_list_remove(&surface->commit.link);
}

static void unmanaged_handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *surface =
		wl_container_of(listener, surface, destroy);
	struct wlr_xwayland_surface *xsurface = surface->wlr_xwayland_surface;
	if (xsurface->mapped) {
		unmanaged_handle_unmap(&surface->unmap, xsurface);
	}
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
		wlr_log(L_ERROR, "Allocation failed");
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

	unmanaged_handle_map(&surface->map, xsurface);

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

static const char *get_prop(struct sway_view *view, enum sway_view_prop prop) {
	if (xwayland_view_from_view(view) == NULL) {
		return NULL;
	}
	switch (prop) {
	case VIEW_PROP_TITLE:
		return view->wlr_xwayland_surface->title;
	case VIEW_PROP_CLASS:
		return view->wlr_xwayland_surface->class;
	default:
		return NULL;
	}
}

static void configure(struct sway_view *view, double ox, double oy, int width,
		int height) {
	struct sway_xwayland_view *xwayland_view = xwayland_view_from_view(view);
	if (xwayland_view == NULL) {
		return;
	}
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;

	struct sway_container *output = container_parent(view->swayc, C_OUTPUT);
	if (!sway_assert(output, "view must be within tree to set position")) {
		return;
	}
	struct sway_container *root = container_parent(output, C_ROOT);
	if (!sway_assert(root, "output must be within tree to set position")) {
		return;
	}
	struct wlr_output_layout *layout = root->sway_root->output_layout;
	struct wlr_output_layout_output *loutput =
		wlr_output_layout_get(layout, output->sway_output->wlr_output);
	if (!sway_assert(loutput, "output must be within layout to set position")) {
		return;
	}

	view_update_position(view, ox, oy);

	xwayland_view->pending_width = width;
	xwayland_view->pending_height = height;
	wlr_xwayland_surface_configure(xsurface, ox + loutput->x, oy + loutput->y,
		width, height);
}

static void set_activated(struct sway_view *view, bool activated) {
	if (xwayland_view_from_view(view) == NULL) {
		return;
	}
	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;
	wlr_xwayland_surface_activate(surface, activated);
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
	wl_list_remove(&xwayland_view->destroy.link);
	wl_list_remove(&xwayland_view->request_configure.link);
	wl_list_remove(&xwayland_view->map.link);
	wl_list_remove(&xwayland_view->unmap.link);
	free(xwayland_view);
}

static const struct sway_view_impl view_impl = {
	.get_prop = get_prop,
	.configure = configure,
	.set_activated = set_activated,
	.close = _close,
	.destroy = destroy,
};

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, commit);
	struct sway_view *view = &xwayland_view->view;
	// NOTE: We intentionally discard the view's desired width here
	// TODO: Let floating views do whatever
	view_update_size(view, xwayland_view->pending_width,
		xwayland_view->pending_height);
	view_damage_from(view);
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, unmap);
	wl_list_remove(&xwayland_view->commit.link);
	view_unmap(&xwayland_view->view);
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, map);
	struct wlr_xwayland_surface *xsurface = data;
	struct sway_view *view = &xwayland_view->view;

	// Wire up the commit listener here, because xwayland map/unmap can change
	// the underlying wlr_surface
	wl_signal_add(&xsurface->surface->events.commit, &xwayland_view->commit);
	xwayland_view->commit.notify = handle_commit;

	// Put it back into the tree
	wlr_xwayland_surface_set_maximized(xsurface, true);
	view_map(view, xsurface->surface);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, destroy);
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	if (xsurface->mapped) {
		handle_unmap(&xwayland_view->unmap, xsurface);
	}
	view_destroy(&xwayland_view->view);
}

static void handle_request_configure(struct wl_listener *listener, void *data) {
	struct sway_xwayland_view *xwayland_view =
		wl_container_of(listener, xwayland_view, request_configure);
	struct wlr_xwayland_surface_configure_event *ev = data;
	struct sway_view *view = &xwayland_view->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	// TODO: floating windows are allowed to move around like this, but make
	// sure tiling windows always stay in place.
	wlr_xwayland_surface_configure(xsurface, ev->x, ev->y,
		ev->width, ev->height);
}

void handle_xwayland_surface(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(listener, server,
		xwayland_surface);
	struct wlr_xwayland_surface *xsurface = data;

	if (wlr_xwayland_surface_is_unmanaged(xsurface) ||
			xsurface->override_redirect) {
		wlr_log(L_DEBUG, "New xwayland unmanaged surface");
		create_unmanaged(xsurface);
		return;
	}

	wlr_log(L_DEBUG, "New xwayland surface title='%s' class='%s'",
		xsurface->title, xsurface->class);

	struct sway_xwayland_view *xwayland_view =
		calloc(1, sizeof(struct sway_xwayland_view));
	if (!sway_assert(xwayland_view, "Failed to allocate view")) {
		return;
	}

	view_init(&xwayland_view->view, SWAY_VIEW_XWAYLAND, &view_impl);
	xwayland_view->view.wlr_xwayland_surface = xsurface;

	// TODO:
	// - Look up pid and open on appropriate workspace
	// - Criteria

	wl_signal_add(&xsurface->events.destroy, &xwayland_view->destroy);
	xwayland_view->destroy.notify = handle_destroy;

	wl_signal_add(&xsurface->events.request_configure,
		&xwayland_view->request_configure);
	xwayland_view->request_configure.notify = handle_request_configure;

	wl_signal_add(&xsurface->events.unmap, &xwayland_view->unmap);
	xwayland_view->unmap.notify = handle_unmap;

	wl_signal_add(&xsurface->events.map, &xwayland_view->map);
	xwayland_view->map.notify = handle_map;

	handle_map(&xwayland_view->map, xsurface);
}
