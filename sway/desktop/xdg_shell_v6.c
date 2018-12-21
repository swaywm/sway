#define _POSIX_C_SOURCE 199309L
#include <float.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include "log.h"
#include "sway/decoration.h"
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

static const struct sway_view_child_impl popup_impl;

static void popup_get_root_coords(struct sway_view_child *child,
		int *root_sx, int *root_sy) {
	struct sway_xdg_popup_v6 *popup = (struct sway_xdg_popup_v6 *)child;
	struct wlr_xdg_surface_v6 *surface = popup->wlr_xdg_surface_v6;

	int x_offset = -child->view->geometry.x - surface->geometry.x;
	int y_offset = -child->view->geometry.y - surface->geometry.y;

	wlr_xdg_popup_v6_get_toplevel_coords(surface->popup,
		x_offset + surface->popup->geometry.x,
		y_offset + surface->popup->geometry.y,
		root_sx, root_sy);
}

static void popup_destroy(struct sway_view_child *child) {
	if (!sway_assert(child->impl == &popup_impl,
			"Expected an xdg_shell_v6 popup")) {
		return;
	}
	struct sway_xdg_popup_v6 *popup = (struct sway_xdg_popup_v6 *)child;
	wl_list_remove(&popup->new_popup.link);
	wl_list_remove(&popup->destroy.link);
	free(popup);
}

static const struct sway_view_child_impl popup_impl = {
	.get_root_coords = popup_get_root_coords,
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

static void popup_handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xdg_popup_v6 *popup = wl_container_of(listener, popup, destroy);
	view_child_destroy(&popup->child);
}

static void popup_unconstrain(struct sway_xdg_popup_v6 *popup) {
	struct sway_view *view = popup->child.view;
	struct wlr_xdg_popup_v6 *wlr_popup = popup->wlr_xdg_surface_v6->popup;

	struct sway_output *output = view->container->workspace->output;

	// the output box expressed in the coordinate system of the toplevel parent
	// of the popup
	struct wlr_box output_toplevel_sx_box = {
		.x = output->lx - view->container->content_x,
		.y = output->ly - view->container->content_y,
		.width = output->width,
		.height = output->height,
	};

	wlr_xdg_popup_v6_unconstrain_from_box(wlr_popup, &output_toplevel_sx_box);
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
	popup->wlr_xdg_surface_v6 = xdg_surface;

	wl_signal_add(&xdg_surface->events.new_popup, &popup->new_popup);
	popup->new_popup.notify = popup_handle_new_popup;
	wl_signal_add(&xdg_surface->events.destroy, &popup->destroy);
	popup->destroy.notify = popup_handle_destroy;

	wl_signal_add(&xdg_surface->events.map, &popup->child.surface_map);
	wl_signal_add(&xdg_surface->events.unmap, &popup->child.surface_unmap);

	popup_unconstrain(popup);

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

static void get_constraints(struct sway_view *view, double *min_width,
		double *max_width, double *min_height, double *max_height) {
	struct wlr_xdg_toplevel_v6_state *state =
		&view->wlr_xdg_surface_v6->toplevel->current;
	*min_width = state->min_width > 0 ? state->min_width : DBL_MIN;
	*max_width = state->max_width > 0 ? state->max_width : DBL_MAX;
	*min_height = state->min_height > 0 ? state->min_height : DBL_MIN;
	*max_height = state->max_height > 0 ? state->max_height : DBL_MAX;
}

static const char *get_string_prop(struct sway_view *view,
		enum sway_view_prop prop) {
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

static uint32_t configure(struct sway_view *view, double lx, double ly,
		int width, int height) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		xdg_shell_v6_view_from_view(view);
	if (xdg_shell_v6_view == NULL) {
		return 0;
	}
	return wlr_xdg_toplevel_v6_set_size(
			view->wlr_xdg_surface_v6, width, height);
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

static void set_tiled(struct sway_view *view, bool tiled) {
	if (xdg_shell_v6_view_from_view(view) == NULL) {
		return;
	}
	struct wlr_xdg_surface_v6 *surface = view->wlr_xdg_surface_v6;
	wlr_xdg_toplevel_v6_set_maximized(surface, tiled);
}

static void set_fullscreen(struct sway_view *view, bool fullscreen) {
	if (xdg_shell_v6_view_from_view(view) == NULL) {
		return;
	}
	struct wlr_xdg_surface_v6 *surface = view->wlr_xdg_surface_v6;
	wlr_xdg_toplevel_v6_set_fullscreen(surface, fullscreen);
}

static bool wants_floating(struct sway_view *view) {
	struct wlr_xdg_toplevel_v6 *toplevel =
		view->wlr_xdg_surface_v6->toplevel;
	struct wlr_xdg_toplevel_v6_state *state = &toplevel->current;
	return (state->min_width != 0 && state->min_height != 0
		&& (state->min_width == state->max_width
		|| state->min_height == state->max_height))
		|| toplevel->parent;
}

static void for_each_surface(struct sway_view *view,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	if (xdg_shell_v6_view_from_view(view) == NULL) {
		return;
	}
	wlr_xdg_surface_v6_for_each_surface(view->wlr_xdg_surface_v6, iterator,
		user_data);
}

static void for_each_popup(struct sway_view *view,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	if (xdg_shell_v6_view_from_view(view) == NULL) {
		return;
	}
	wlr_xdg_surface_v6_for_each_popup(view->wlr_xdg_surface_v6, iterator,
		user_data);
}

static bool is_transient_for(struct sway_view *child,
		struct sway_view *ancestor) {
	if (xdg_shell_v6_view_from_view(child) == NULL) {
		return false;
	}
	struct wlr_xdg_surface_v6 *surface = child->wlr_xdg_surface_v6;
	while (surface && surface->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL) {
		if (surface->toplevel->parent == ancestor->wlr_xdg_surface_v6) {
			return true;
		}
		surface = surface->toplevel->parent;
	}
	return false;
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

static void close_popups_iterator(struct wlr_surface *surface,
		int sx, int sy, void *data) {
	struct wlr_xdg_surface_v6 *xdg_surface_v6 =
		wlr_xdg_surface_v6_from_wlr_surface(surface);
	wlr_xdg_surface_v6_send_close(xdg_surface_v6);
}

static void close_popups(struct sway_view *view) {
	wlr_xdg_surface_v6_for_each_popup(view->wlr_xdg_surface_v6,
			close_popups_iterator, NULL);
}

static void destroy(struct sway_view *view) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		xdg_shell_v6_view_from_view(view);
	if (xdg_shell_v6_view == NULL) {
		return;
	}
	free(xdg_shell_v6_view);
}

static const struct sway_view_impl view_impl = {
	.get_constraints = get_constraints,
	.get_string_prop = get_string_prop,
	.configure = configure,
	.set_activated = set_activated,
	.set_tiled = set_tiled,
	.set_fullscreen = set_fullscreen,
	.wants_floating = wants_floating,
	.for_each_surface = for_each_surface,
	.for_each_popup = for_each_popup,
	.is_transient_for = is_transient_for,
	.close = _close,
	.close_popups = close_popups,
	.destroy = destroy,
};

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, commit);
	struct sway_view *view = &xdg_shell_v6_view->view;
	struct wlr_xdg_surface_v6 *xdg_surface_v6 = view->wlr_xdg_surface_v6;

	if (view->container->node.instruction) {
		wlr_xdg_surface_v6_get_geometry(xdg_surface_v6, &view->geometry);
		transaction_notify_view_ready_by_serial(view,
				xdg_surface_v6->configure_serial);
	} else {
		struct wlr_box new_geo;
		wlr_xdg_surface_v6_get_geometry(xdg_surface_v6, &new_geo);
		struct sway_container *con = view->container;

		if ((new_geo.width != con->surface_width ||
					new_geo.height != con->surface_height)) {
			// The view has unexpectedly sent a new size
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

static void handle_set_title(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, set_title);
	struct sway_view *view = &xdg_shell_v6_view->view;
	view_update_title(view, false);
	view_execute_criteria(view);
}

static void handle_set_app_id(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, set_app_id);
	struct sway_view *view = &xdg_shell_v6_view->view;
	view_execute_criteria(view);
}

static void handle_new_popup(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, new_popup);
	struct wlr_xdg_popup_v6 *wlr_popup = data;
	popup_create(wlr_popup, &xdg_shell_v6_view->view);
}

static void handle_request_fullscreen(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, request_fullscreen);
	struct wlr_xdg_toplevel_v6_set_fullscreen_event *e = data;
	struct wlr_xdg_surface_v6 *xdg_surface =
		xdg_shell_v6_view->view.wlr_xdg_surface_v6;
	struct sway_view *view = &xdg_shell_v6_view->view;

	if (!sway_assert(xdg_surface->role == WLR_XDG_SURFACE_V6_ROLE_TOPLEVEL,
				"xdg_shell_v6 requested fullscreen of surface with role %i",
				xdg_surface->role)) {
		return;
	}
	if (!xdg_surface->mapped) {
		return;
	}

	container_set_fullscreen(view->container, e->fullscreen);

	arrange_root();
	transaction_commit_dirty();
}

static void handle_request_move(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, request_move);
	struct sway_view *view = &xdg_shell_v6_view->view;
	if (!container_is_floating(view->container)) {
		return;
	}
	struct wlr_xdg_toplevel_v6_move_event *e = data;
	struct sway_seat *seat = e->seat->seat->data;
	if (e->serial == seat->last_button_serial) {
		seatop_begin_move_floating(seat, view->container, seat->last_button);
	}
}

static void handle_request_resize(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, request_resize);
	struct sway_view *view = &xdg_shell_v6_view->view;
	if (!container_is_floating(view->container)) {
		return;
	}
	struct wlr_xdg_toplevel_v6_resize_event *e = data;
	struct sway_seat *seat = e->seat->seat->data;
	if (e->serial == seat->last_button_serial) {
		seatop_begin_resize_floating(seat, view->container,
				seat->last_button, e->edges);
	}
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, unmap);
	struct sway_view *view = &xdg_shell_v6_view->view;

	if (!sway_assert(view->surface, "Cannot unmap unmapped view")) {
		return;
	}

	view_unmap(view);

	wl_list_remove(&xdg_shell_v6_view->commit.link);
	wl_list_remove(&xdg_shell_v6_view->new_popup.link);
	wl_list_remove(&xdg_shell_v6_view->request_fullscreen.link);
	wl_list_remove(&xdg_shell_v6_view->request_move.link);
	wl_list_remove(&xdg_shell_v6_view->request_resize.link);
	wl_list_remove(&xdg_shell_v6_view->set_title.link);
	wl_list_remove(&xdg_shell_v6_view->set_app_id.link);
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, map);
	struct sway_view *view = &xdg_shell_v6_view->view;
	struct wlr_xdg_surface_v6 *xdg_surface = view->wlr_xdg_surface_v6;

	view->natural_width = view->wlr_xdg_surface_v6->geometry.width;
	view->natural_height = view->wlr_xdg_surface_v6->geometry.height;
	if (!view->natural_width && !view->natural_height) {
		view->natural_width = view->wlr_xdg_surface_v6->surface->current.width;
		view->natural_height = view->wlr_xdg_surface_v6->surface->current.height;
	}
	struct sway_server_decoration *deco =
			decoration_from_surface(xdg_surface->surface);
	bool csd = !deco || deco->wlr_server_decoration->mode
		== WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT;

	view_map(view, view->wlr_xdg_surface_v6->surface,
		xdg_surface->toplevel->client_pending.fullscreen, csd);

	transaction_commit_dirty();

	xdg_shell_v6_view->commit.notify = handle_commit;
	wl_signal_add(&xdg_surface->surface->events.commit,
		&xdg_shell_v6_view->commit);

	xdg_shell_v6_view->new_popup.notify = handle_new_popup;
	wl_signal_add(&xdg_surface->events.new_popup,
		&xdg_shell_v6_view->new_popup);

	xdg_shell_v6_view->request_fullscreen.notify = handle_request_fullscreen;
	wl_signal_add(&xdg_surface->toplevel->events.request_fullscreen,
			&xdg_shell_v6_view->request_fullscreen);

	xdg_shell_v6_view->request_move.notify = handle_request_move;
	wl_signal_add(&xdg_surface->toplevel->events.request_move,
			&xdg_shell_v6_view->request_move);

	xdg_shell_v6_view->request_resize.notify = handle_request_resize;
	wl_signal_add(&xdg_surface->toplevel->events.request_resize,
			&xdg_shell_v6_view->request_resize);

	xdg_shell_v6_view->set_title.notify = handle_set_title;
	wl_signal_add(&xdg_surface->toplevel->events.set_title,
			&xdg_shell_v6_view->set_title);

	xdg_shell_v6_view->set_app_id.notify = handle_set_app_id;
	wl_signal_add(&xdg_surface->toplevel->events.set_app_id,
			&xdg_shell_v6_view->set_app_id);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		wl_container_of(listener, xdg_shell_v6_view, destroy);
	struct sway_view *view = &xdg_shell_v6_view->view;
	wl_list_remove(&xdg_shell_v6_view->destroy.link);
	wl_list_remove(&xdg_shell_v6_view->map.link);
	wl_list_remove(&xdg_shell_v6_view->unmap.link);
	view->wlr_xdg_surface_v6 = NULL;
	view_begin_destroy(view);
}

struct sway_view *view_from_wlr_xdg_surface_v6(
		struct wlr_xdg_surface_v6 *xdg_surface_v6) {
       return xdg_surface_v6->data;
}

void handle_xdg_shell_v6_surface(struct wl_listener *listener, void *data) {
	struct wlr_xdg_surface_v6 *xdg_surface = data;

	if (xdg_surface->role == WLR_XDG_SURFACE_V6_ROLE_POPUP) {
		sway_log(SWAY_DEBUG, "New xdg_shell_v6 popup");
		return;
	}

	sway_log(SWAY_DEBUG, "New xdg_shell_v6 toplevel title='%s' app_id='%s'",
		xdg_surface->toplevel->title, xdg_surface->toplevel->app_id);
	wlr_xdg_surface_v6_ping(xdg_surface);

	struct sway_xdg_shell_v6_view *xdg_shell_v6_view =
		calloc(1, sizeof(struct sway_xdg_shell_v6_view));
	if (!sway_assert(xdg_shell_v6_view, "Failed to allocate view")) {
		return;
	}

	view_init(&xdg_shell_v6_view->view, SWAY_VIEW_XDG_SHELL_V6, &view_impl);
	xdg_shell_v6_view->view.wlr_xdg_surface_v6 = xdg_surface;

	xdg_shell_v6_view->map.notify = handle_map;
	wl_signal_add(&xdg_surface->events.map, &xdg_shell_v6_view->map);

	xdg_shell_v6_view->unmap.notify = handle_unmap;
	wl_signal_add(&xdg_surface->events.unmap, &xdg_shell_v6_view->unmap);

	xdg_shell_v6_view->destroy.notify = handle_destroy;
	wl_signal_add(&xdg_surface->events.destroy, &xdg_shell_v6_view->destroy);

	xdg_surface->data = xdg_shell_v6_view;
}
