#define _POSIX_C_SOURCE 199309L
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/xwayland.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/server.h"
#include "sway/tree/view.h"
#include "sway/output.h"
#include "sway/input/seat.h"
#include "sway/input/input-manager.h"
#include "log.h"

static void unmanaged_handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xwayland_unmanaged *sway_surface =
		wl_container_of(listener, sway_surface, destroy);
	wl_list_remove(&sway_surface->destroy.link);
	wl_list_remove(&sway_surface->link);
	free(sway_surface);
}

static void create_unmanaged(struct wlr_xwayland_surface *xsurface) {
	struct sway_xwayland_unmanaged *sway_surface =
		calloc(1, sizeof(struct sway_xwayland_unmanaged));
	if (!sway_assert(sway_surface, "Failed to allocate surface")) {
		return;
	}

	sway_surface->wlr_xwayland_surface = xsurface;

	wl_signal_add(&xsurface->events.destroy, &sway_surface->destroy);
	sway_surface->destroy.notify = unmanaged_handle_destroy;

	wl_list_insert(&root_container.sway_root->xwayland_unmanaged,
		&sway_surface->link);

	// TODO: damage tracking
}


static bool assert_xwayland(struct sway_view *view) {
	return sway_assert(view->type == SWAY_XWAYLAND_VIEW,
		"Expected xwayland view!");
}

static const char *get_prop(struct sway_view *view, enum sway_view_prop prop) {
	if (!assert_xwayland(view)) {
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
	if (!assert_xwayland(view)) {
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

	view->sway_xwayland_surface->pending_width = width;
	view->sway_xwayland_surface->pending_height = height;
	wlr_xwayland_surface_configure(xsurface, ox + loutput->x, oy + loutput->y,
		width, height);
}

static void set_activated(struct sway_view *view, bool activated) {
	if (!assert_xwayland(view)) {
		return;
	}
	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;
	wlr_xwayland_surface_activate(surface, activated);
}

static void _close(struct sway_view *view) {
	if (!assert_xwayland(view)) {
		return;
	}
	wlr_xwayland_surface_close(view->wlr_xwayland_surface);
}

static const struct sway_view_impl view_impl = {
	.get_prop = get_prop,
	.configure = configure,
	.set_activated = set_activated,
	.close = _close,
};

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_xwayland_surface *sway_surface =
		wl_container_of(listener, sway_surface, commit);
	struct sway_view *view = sway_surface->view;
	// NOTE: We intentionally discard the view's desired width here
	// TODO: Let floating views do whatever
	view_update_size(view, sway_surface->pending_width,
		sway_surface->pending_height);
	view_damage_from(view);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xwayland_surface *sway_surface =
		wl_container_of(listener, sway_surface, destroy);
	wl_list_remove(&sway_surface->commit.link);
	wl_list_remove(&sway_surface->destroy.link);
	wl_list_remove(&sway_surface->request_configure.link);
	wl_list_remove(&sway_surface->map.link);
	wl_list_remove(&sway_surface->unmap.link);
	view_destroy(sway_surface->view);
	free(sway_surface);
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_xwayland_surface *sway_surface =
		wl_container_of(listener, sway_surface, unmap);
	view_unmap(sway_surface->view);
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct sway_xwayland_surface *sway_surface =
		wl_container_of(listener, sway_surface, map);
	struct wlr_xwayland_surface *xsurface = data;
	struct sway_view *view = sway_surface->view;

	// put it back into the tree
	wlr_xwayland_surface_set_maximized(xsurface, true);
	view_map(view, xsurface->surface);
}

static void handle_request_configure(struct wl_listener *listener, void *data) {
	struct sway_xwayland_surface *sway_surface =
		wl_container_of(listener, sway_surface, request_configure);
	struct wlr_xwayland_surface_configure_event *ev = data;
	struct sway_view *view = sway_surface->view;
	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	// TODO: floating windows are allowed to move around like this, but make
	// sure tiling windows always stay in place.
	wlr_xwayland_surface_configure(xsurface, ev->x, ev->y,
		ev->width, ev->height);
}

void handle_xwayland_surface(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(
			listener, server, xwayland_surface);
	struct wlr_xwayland_surface *xsurface = data;

	if (wlr_xwayland_surface_is_unmanaged(xsurface) ||
			xsurface->override_redirect) {
		wlr_log(L_DEBUG, "New xwayland unmanaged surface");
		create_unmanaged(xsurface);
		return;
	}

	wlr_log(L_DEBUG, "New xwayland surface title='%s' class='%s'",
		xsurface->title, xsurface->class);

	struct sway_xwayland_surface *sway_surface =
		calloc(1, sizeof(struct sway_xwayland_surface));
	if (!sway_assert(sway_surface, "Failed to allocate surface")) {
		return;
	}

	struct sway_view *view = view_create(SWAY_XWAYLAND_VIEW, &view_impl);
	if (!sway_assert(view, "Failed to allocate view")) {
		return;
	}
	view->wlr_xwayland_surface = xsurface;
	view->sway_xwayland_surface = sway_surface;
	sway_surface->view = view;

	// TODO:
	// - Look up pid and open on appropriate workspace
	// - Criteria

	wl_signal_add(&xsurface->surface->events.commit, &sway_surface->commit);
	sway_surface->commit.notify = handle_commit;

	wl_signal_add(&xsurface->events.destroy, &sway_surface->destroy);
	sway_surface->destroy.notify = handle_destroy;

	wl_signal_add(&xsurface->events.request_configure,
		&sway_surface->request_configure);
	sway_surface->request_configure.notify = handle_request_configure;

	wl_signal_add(&xsurface->events.unmap, &sway_surface->unmap);
	sway_surface->unmap.notify = handle_unmap;

	wl_signal_add(&xsurface->events.map, &sway_surface->map);
	sway_surface->map.notify = handle_map;

	handle_map(&sway_surface->map, xsurface);
}
