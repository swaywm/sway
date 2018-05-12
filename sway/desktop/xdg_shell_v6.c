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

static const struct sway_view_child_impl popup_impl;

static void popup_destroy(struct sway_view_child *child) {
	if (!sway_assert(child->impl == &popup_impl,
			"Expected an xdg_shell_v6 popup")) {
		return;
	}
	struct sway_xdg_popup_v6 *popup = (struct sway_xdg_popup_v6 *)child;
	wl_list_remove(&popup->new_popup.link);
	wl_list_remove(&popup->unmap.link);
	wl_list_remove(&popup->destroy.link);
	free(popup);
}

static const struct sway_view_child_impl popup_impl = {
	.destroy = popup_destroy,
};

static struct sway_xdg_popup_v6 *popup_create(
	struct wlr_xdg_popup_v6 *wlr_popup, struct sway_view *view);

static void popup_handle_new_popup(struct wl_listener *listener, void *data) {
	struct sway_xdg_popup_v6 *popup =
		wl_container_of(listener, popup, new_popup);
	struct wlr_xdg_popup_v6 *wlr_popup = data;
	popup_create(wlr_popup, popup->child.view);
}

static void popup_handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_xdg_popup_v6 *popup = wl_container_of(listener, popup, unmap);
	view_child_destroy(&popup->child);
}

static void popup_handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xdg_popup_v6 *popup = wl_container_of(listener, popup, destroy);
	view_child_destroy(&popup->child);
}

static struct sway_xdg_popup_v6 *popup_create(
		struct wlr_xdg_popup_v6 *wlr_popup, struct sway_view *view) {
	struct wlr_xdg_surface_v6 *xdg_surface = wlr_popup->base;

	struct sway_xdg_popup_v6 *popup =
		calloc(1, sizeof(struct sway_xdg_popup_v6));
	if (popup == NULL) {
		return NULL;
	}
	view_child_init(&popup->child, &popup_impl, view, xdg_surface->surface);

	wl_signal_add(&xdg_surface->events.new_popup, &popup->new_popup);
	popup->new_popup.notify = popup_handle_new_popup;
	wl_signal_add(&xdg_surface->events.unmap, &popup->unmap);
	popup->unmap.notify = popup_handle_unmap;
	wl_signal_add(&xdg_surface->events.destroy, &popup->destroy);
	popup->destroy.notify = popup_handle_destroy;

	return popup;
}


static struct sway_xdg_shell_v6_view *xdg_shell_v6_view_from_view(
		struct sway_view *view) {
	if (!sway_assert(view->type == SWAY_VIEW_XDG_SHELL_V6,
			"Expected xdg_shell_v6 view")) {
		return NULL;
	}
	return (struct sway_xdg_shell_v6_view *)view;
}

static const char *get_prop(struct sway_view *view, enum sway_view_prop prop) {
	if (xdg_shell_v6_view_from_view(view) == NULL) {
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
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		xdg_shell_v6_view_from_view(view);
	if (xdg_shell_v6_view == NULL) {
		return;
	}

	xdg_shell_v6_view->pending_width = width;
	xdg_shell_v6_view->pending_height = height;
	wlr_xdg_toplevel_v6_set_size(view->wlr_xdg_surface_v6, width, height);
}

static void set_activated(struct sway_view *view, bool activated) {
	if (xdg_shell_v6_view_from_view(view) == NULL) {
		return;
	}
	struct wlr_xdg_surface_v6 *surface = view->wlr_xdg_surface_v6;
	if (surface->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
		wlr_xdg_toplevel_v6_set_activated(surface, activated);
	}
}

static void set_fullscreen(struct sway_view *view, bool fullscreen) {
	if (xdg_shell_v6_view_from_view(view) == NULL) {
		return;
	}
	struct wlr_xdg_surface_v6 *surface = view->wlr_xdg_surface_v6;
	wlr_xdg_toplevel_v6_set_fullscreen(surface, fullscreen);
}

static void for_each_surface(struct sway_view *view,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	if (xdg_shell_v6_view_from_view(view) == NULL) {
		return;
	}
	wlr_xdg_surface_v6_for_each_surface(view->wlr_xdg_surface_v6, iterator,
		user_data);
}

static void _close(struct sway_view *view) {
	if (xdg_shell_v6_view_from_view(view) == NULL) {
		return;
	}
	struct wlr_xdg_surface_v6 *surface = view->wlr_xdg_surface_v6;
	if (surface->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
		wlr_xdg_surface_v6_send_close(surface);
	}
}

static void destroy(struct sway_view *view) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		xdg_shell_v6_view_from_view(view);
	if (xdg_shell_v6_view == NULL) {
		return;
	}
	wl_list_remove(&xdg_shell_v6_view->destroy.link);
	wl_list_remove(&xdg_shell_v6_view->map.link);
	wl_list_remove(&xdg_shell_v6_view->unmap.link);
	wl_list_remove(&xdg_shell_v6_view->request_fullscreen.link);
	free(xdg_shell_v6_view);
}

static const struct sway_view_impl view_impl = {
	.get_prop = get_prop,
	.configure = configure,
	.set_activated = set_activated,
	.set_fullscreen = set_fullscreen,
	.for_each_surface = for_each_surface,
	.close = _close,
	.destroy = destroy,
};

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, commit);
	struct sway_view *view = &xdg_shell_v6_view->view;
	// NOTE: We intentionally discard the view's desired width here
	// TODO: Store this for restoration when moving to floating plane
	// TODO: Let floating views do whatever
	view_update_size(view, xdg_shell_v6_view->pending_width,
		xdg_shell_v6_view->pending_height);
	view_update_title(view, false);
	view_damage_from(view);
}

static void handle_new_popup(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, new_popup);
	struct wlr_xdg_popup_v6 *wlr_popup = data;
	popup_create(wlr_popup, &xdg_shell_v6_view->view);
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, unmap);

	view_unmap(&xdg_shell_v6_view->view);

	wl_list_remove(&xdg_shell_v6_view->commit.link);
	wl_list_remove(&xdg_shell_v6_view->new_popup.link);
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, map);
	struct sway_view *view = &xdg_shell_v6_view->view;
	struct wlr_xdg_surface_v6 *xdg_surface = view->wlr_xdg_surface_v6;

	view_map(view, view->wlr_xdg_surface_v6->surface);

	xdg_shell_v6_view->commit.notify = handle_commit;
	wl_signal_add(&xdg_surface->surface->events.commit,
		&xdg_shell_v6_view->commit);

	xdg_shell_v6_view->new_popup.notify = handle_new_popup;
	wl_signal_add(&xdg_surface->events.new_popup,
		&xdg_shell_v6_view->new_popup);

	if (xdg_surface->toplevel->client_pending.fullscreen) {
		view_set_fullscreen(view, true);
	}
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, destroy);
	view_destroy(&xdg_shell_v6_view->view);
}

static void handle_request_fullscreen(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, request_fullscreen);
	struct wlr_xdg_toplevel_v6_set_fullscreen_event *e = data;
	struct wlr_xdg_surface_v6 *xdg_surface =
		xdg_shell_v6_view->view.wlr_xdg_surface_v6;

	if (!sway_assert(xdg_surface->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL,
				"xdg_shell_v6 requested fullscreen of surface with role %i",
				xdg_surface->role)) {
		return;
	}
	if (!xdg_surface->mapped) {
		return;
	}

	view_set_fullscreen(&xdg_shell_v6_view->view, e->fullscreen);
}

void handle_xdg_shell_v6_surface(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(listener, server,
		xdg_shell_v6_surface);
	struct wlr_xdg_surface_v6 *xdg_surface = data;

	if (xdg_surface->role == WLR_XDG_SURFACE_V6_ROLE_POPUP) {
		wlr_log(L_DEBUG, "New xdg_shell_v6 popup");
		return;
	}

	wlr_log(L_DEBUG, "New xdg_shell_v6 toplevel title='%s' app_id='%s'",
		xdg_surface->toplevel->title, xdg_surface->toplevel->app_id);
	wlr_xdg_surface_v6_ping(xdg_surface);
	wlr_xdg_toplevel_v6_set_maximized(xdg_surface, true);

	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		calloc(1, sizeof(struct sway_xdg_shell_v6_view));
	if (!sway_assert(xdg_shell_v6_view, "Failed to allocate view")) {
		return;
	}

	view_init(&xdg_shell_v6_view->view, SWAY_VIEW_XDG_SHELL_V6, &view_impl);
	xdg_shell_v6_view->view.wlr_xdg_surface_v6 = xdg_surface;

	// TODO:
	// - Look up pid and open on appropriate workspace

	xdg_shell_v6_view->map.notify = handle_map;
	wl_signal_add(&xdg_surface->events.map, &xdg_shell_v6_view->map);

	xdg_shell_v6_view->unmap.notify = handle_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &xdg_shell_v6_view->unmap);

	xdg_shell_v6_view->destroy.notify = handle_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &xdg_shell_v6_view->destroy);

	xdg_shell_v6_view->request_fullscreen.notify = handle_request_fullscreen;
	wl_signal_add(&xdg_surface->toplevel->events.request_fullscreen,
			&xdg_shell_v6_view->request_fullscreen);
}
