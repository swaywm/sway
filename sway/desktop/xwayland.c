#define _POSIX_C_SOURCE 199309L
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/xwayland.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include "sway/container.h"
#include "sway/layout.h"
#include "sway/server.h"
#include "sway/view.h"
#include "sway/output.h"
#include "sway/input/seat.h"
#include "sway/input/input-manager.h"
#include "log.h"

 static bool assert_xwayland(struct sway_view *view) {
	 return sway_assert(view->type == SWAY_XWAYLAND_VIEW && view->wlr_xwayland_surface,
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

static void set_size(struct sway_view *view, int width, int height) {
	if (!assert_xwayland(view)) {
		return;
	}
	view->sway_xwayland_surface->pending_width = width;
	view->sway_xwayland_surface->pending_height = height;

	struct wlr_xwayland_surface *xsurface = view->wlr_xwayland_surface;
	wlr_xwayland_surface_configure(xsurface, view->swayc->x, view->swayc->y,
		width, height);
}

static void set_position(struct sway_view *view, double ox, double oy) {
	if (!assert_xwayland(view)) {
		return;
	}
	swayc_t *output = swayc_parent_by_type(view->swayc, C_OUTPUT);
	if (!sway_assert(output, "view must be within tree to set position")) {
		return;
	}
	swayc_t *root = swayc_parent_by_type(output, C_ROOT);
	if (!sway_assert(root, "output must be within tree to set position")) {
		return;
	}
	struct wlr_output_layout *layout = root->sway_root->output_layout;
	struct wlr_output_layout_output *loutput =
		wlr_output_layout_get(layout, output->sway_output->wlr_output);
	if (!sway_assert(loutput, "output must be within layout to set position")) {
		return;
	}

	view->swayc->x = ox;
	view->swayc->y = oy;

	wlr_xwayland_surface_configure(view->wlr_xwayland_surface,
		ox + loutput->x, oy + loutput->y,
		view->width, view->height);
}

static void set_activated(struct sway_view *view, bool activated) {
	if (!assert_xwayland(view)) {
		return;
	}
	struct wlr_xwayland_surface *surface = view->wlr_xwayland_surface;
	wlr_xwayland_surface_activate(surface, activated);
}

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_xwayland_surface *sway_surface =
		wl_container_of(listener, sway_surface, commit);
	struct sway_view *view = sway_surface->view;
	// NOTE: We intentionally discard the view's desired width here
	// TODO: Let floating views do whatever
	view->width = sway_surface->pending_width;
	view->height = sway_surface->pending_height;
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xwayland_surface *sway_surface =
		wl_container_of(listener, sway_surface, destroy);
	wl_list_remove(&sway_surface->commit.link);
	wl_list_remove(&sway_surface->destroy.link);
	wl_list_remove(&sway_surface->request_configure.link);
	swayc_t *parent = destroy_view(sway_surface->view->swayc);
	free(sway_surface->view);
	free(sway_surface);
	arrange_windows(parent, -1, -1);
}

static void handle_configure_request(struct wl_listener *listener, void *data) {
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

	if (xsurface->override_redirect) {
		// TODO: floating popups
		return;
	}

	sway_log(L_DEBUG, "New xwayland surface title='%s' class='%s'",
			xsurface->title, xsurface->class);

	struct sway_xwayland_surface *sway_surface =
		calloc(1, sizeof(struct sway_xwayland_surface));
	if (!sway_assert(sway_surface, "Failed to allocate surface!")) {
		return;
	}

	struct sway_view *sway_view = calloc(1, sizeof(struct sway_view));
	if (!sway_assert(sway_view, "Failed to allocate view!")) {
		return;
	}
	sway_view->type = SWAY_XWAYLAND_VIEW;
	sway_view->iface.get_prop = get_prop;
	sway_view->iface.set_size = set_size;
	sway_view->iface.set_position = set_position;
	sway_view->iface.set_activated = set_activated;
	sway_view->wlr_xwayland_surface = xsurface;
	sway_view->sway_xwayland_surface = sway_surface;
	// TODO remove from the tree when the surface goes away (unmapped)
	sway_view->surface = xsurface->surface;
	sway_surface->view = sway_view;

	// TODO:
	// - Wire up listeners
	// - Handle popups
	// - Look up pid and open on appropriate workspace
	// - Set new view to maximized so it behaves nicely
	// - Criteria

	sway_surface->commit.notify = handle_commit;
	wl_signal_add(&xsurface->surface->events.commit, &sway_surface->commit);
	sway_surface->destroy.notify = handle_destroy;
	wl_signal_add(&xsurface->events.destroy, &sway_surface->destroy);
	sway_surface->request_configure.notify = handle_configure_request;
	wl_signal_add(&xsurface->events.request_configure,
		&sway_surface->request_configure);

	// TODO: actual focus semantics
	swayc_t *parent = root_container.children->items[0];
	parent = parent->children->items[0]; // workspace

	swayc_t *cont = new_view(parent, sway_view);
	sway_view->swayc = cont;

	arrange_windows(cont->parent, -1, -1);
	sway_input_manager_set_focus(input_manager, cont);
}
