#include <float.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/edges.h>
#include "log.h"
#include "sway/decoration.h"
#include "sway/scene_descriptor.h"
#include "sway/desktop/transaction.h"
#include "sway/input/cursor.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "sway/xdg_decoration.h"

static struct sway_xdg_popup *popup_create(
	struct wlr_xdg_popup *wlr_popup, struct sway_view *view,
	struct wlr_scene_tree *parent);

static void popup_handle_new_popup(struct wl_listener *listener, void *data) {
	struct sway_xdg_popup *popup =
		wl_container_of(listener, popup, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;
	popup_create(wlr_popup, popup->view, popup->xdg_surface_tree);
}

static void popup_handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xdg_popup *popup = wl_container_of(listener, popup, destroy);

	wl_list_remove(&popup->new_popup.link);
	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->surface_commit.link);
	wl_list_remove(&popup->reposition.link);
	wlr_scene_node_destroy(&popup->scene_tree->node);
	free(popup);
}

static void popup_unconstrain(struct sway_xdg_popup *popup) {
	struct sway_view *view = popup->view;
	struct wlr_xdg_popup *wlr_popup = popup->wlr_xdg_popup;

	struct sway_workspace *workspace = view->container->pending.workspace;
	if (!workspace) {
		// is null if in the scratchpad
		return;
	}

	struct sway_output *output = workspace->output;

	// the output box expressed in the coordinate system of the toplevel parent
	// of the popup
	struct wlr_box output_toplevel_sx_box = {
		.x = output->lx - view->container->pending.content_x + view->geometry.x,
		.y = output->ly - view->container->pending.content_y + view->geometry.y,
		.width = output->width,
		.height = output->height,
	};

	wlr_xdg_popup_unconstrain_from_box(wlr_popup, &output_toplevel_sx_box);
}

static void popup_handle_surface_commit(struct wl_listener *listener, void *data) {
	struct sway_xdg_popup *popup = wl_container_of(listener, popup, surface_commit);
	if (popup->wlr_xdg_popup->base->initial_commit) {
		popup_unconstrain(popup);
	}
}

static void popup_handle_reposition(struct wl_listener *listener, void *data) {
	struct sway_xdg_popup *popup = wl_container_of(listener, popup, reposition);
	popup_unconstrain(popup);
}

static struct sway_xdg_popup *popup_create(struct wlr_xdg_popup *wlr_popup,
		struct sway_view *view, struct wlr_scene_tree *parent) {
	struct wlr_xdg_surface *xdg_surface = wlr_popup->base;

	struct sway_xdg_popup *popup = calloc(1, sizeof(struct sway_xdg_popup));
	if (!popup) {
		return NULL;
	}

	popup->wlr_xdg_popup = wlr_popup;
	popup->view = view;

	popup->scene_tree = wlr_scene_tree_create(parent);
	if (!popup->scene_tree) {
		free(popup);
		return NULL;
	}

	popup->xdg_surface_tree = wlr_scene_xdg_surface_create(
		popup->scene_tree, xdg_surface);
	if (!popup->xdg_surface_tree) {
		wlr_scene_node_destroy(&popup->scene_tree->node);
		free(popup);
		return NULL;
	}

	popup->desc.relative = &view->content_tree->node;
	popup->desc.view = view;

	if (!scene_descriptor_assign(&popup->scene_tree->node,
			SWAY_SCENE_DESC_POPUP, &popup->desc)) {
		sway_log(SWAY_ERROR, "Failed to allocate a popup scene descriptor");
		wlr_scene_node_destroy(&popup->scene_tree->node);
		free(popup);
		return NULL;
	}

	popup->wlr_xdg_popup = xdg_surface->popup;
	struct sway_xdg_shell_view *shell_view =
		wl_container_of(view, shell_view, view);
	xdg_surface->data = shell_view;

	wl_signal_add(&xdg_surface->surface->events.commit, &popup->surface_commit);
	popup->surface_commit.notify = popup_handle_surface_commit;
	wl_signal_add(&xdg_surface->events.new_popup, &popup->new_popup);
	popup->new_popup.notify = popup_handle_new_popup;
	wl_signal_add(&wlr_popup->events.reposition, &popup->reposition);
	popup->reposition.notify = popup_handle_reposition;
	wl_signal_add(&wlr_popup->events.destroy, &popup->destroy);
	popup->destroy.notify = popup_handle_destroy;

	return popup;
}

static struct sway_xdg_shell_view *xdg_shell_view_from_view(
		struct sway_view *view) {
	if (!sway_assert(view->type == SWAY_VIEW_XDG_SHELL,
			"Expected xdg_shell view")) {
		return NULL;
	}
	return (struct sway_xdg_shell_view *)view;
}

static void get_constraints(struct sway_view *view, double *min_width,
		double *max_width, double *min_height, double *max_height) {
	struct wlr_xdg_toplevel_state *state =
		&view->wlr_xdg_toplevel->current;
	*min_width = state->min_width > 0 ? state->min_width : DBL_MIN;
	*max_width = state->max_width > 0 ? state->max_width : DBL_MAX;
	*min_height = state->min_height > 0 ? state->min_height : DBL_MIN;
	*max_height = state->max_height > 0 ? state->max_height : DBL_MAX;
}

static const char *get_string_prop(struct sway_view *view,
		enum sway_view_prop prop) {
	if (xdg_shell_view_from_view(view) == NULL) {
		return NULL;
	}
	switch (prop) {
	case VIEW_PROP_TITLE:
		return view->wlr_xdg_toplevel->title;
	case VIEW_PROP_APP_ID:
		return view->wlr_xdg_toplevel->app_id;
	default:
		return NULL;
	}
}

static uint32_t configure(struct sway_view *view, double lx, double ly,
		int width, int height) {
	struct sway_xdg_shell_view *xdg_shell_view =
		xdg_shell_view_from_view(view);
	if (xdg_shell_view == NULL) {
		return 0;
	}
	return wlr_xdg_toplevel_set_size(view->wlr_xdg_toplevel,
		width, height);
}

static void set_activated(struct sway_view *view, bool activated) {
	if (xdg_shell_view_from_view(view) == NULL) {
		return;
	}
	wlr_xdg_toplevel_set_activated(view->wlr_xdg_toplevel, activated);
}

static void set_tiled(struct sway_view *view, bool tiled) {
	if (xdg_shell_view_from_view(view) == NULL) {
		return;
	}
	if (wl_resource_get_version(view->wlr_xdg_toplevel->resource) >=
			XDG_TOPLEVEL_STATE_TILED_LEFT_SINCE_VERSION) {
		enum wlr_edges edges = WLR_EDGE_NONE;
		if (tiled) {
			edges = WLR_EDGE_LEFT | WLR_EDGE_RIGHT | WLR_EDGE_TOP |
					WLR_EDGE_BOTTOM;
		}
		wlr_xdg_toplevel_set_tiled(view->wlr_xdg_toplevel, edges);
	} else {
		// The version is too low for the tiled state; configure as maximized instead
		// to stop the client from drawing decorations outside of the toplevel geometry.
		wlr_xdg_toplevel_set_maximized(view->wlr_xdg_toplevel, tiled);
	}
}

static void set_fullscreen(struct sway_view *view, bool fullscreen) {
	if (xdg_shell_view_from_view(view) == NULL) {
		return;
	}
	wlr_xdg_toplevel_set_fullscreen(view->wlr_xdg_toplevel, fullscreen);
}

static void set_resizing(struct sway_view *view, bool resizing) {
	if (xdg_shell_view_from_view(view) == NULL) {
		return;
	}
	wlr_xdg_toplevel_set_resizing(view->wlr_xdg_toplevel, resizing);
}

static bool wants_floating(struct sway_view *view) {
	struct wlr_xdg_toplevel *toplevel = view->wlr_xdg_toplevel;
	struct wlr_xdg_toplevel_state *state = &toplevel->current;
	return (state->min_width != 0 && state->min_height != 0
		&& (state->min_width == state->max_width
		|| state->min_height == state->max_height))
		|| toplevel->parent;
}

static bool is_transient_for(struct sway_view *child,
		struct sway_view *ancestor) {
	if (xdg_shell_view_from_view(child) == NULL) {
		return false;
	}
	struct wlr_xdg_toplevel *toplevel = child->wlr_xdg_toplevel;
	while (toplevel) {
		if (toplevel->parent == ancestor->wlr_xdg_toplevel) {
			return true;
		}
		toplevel = toplevel->parent;
	}
	return false;
}

static void _close(struct sway_view *view) {
	if (xdg_shell_view_from_view(view) == NULL) {
		return;
	}
	wlr_xdg_toplevel_send_close(view->wlr_xdg_toplevel);
}

static void close_popups(struct sway_view *view) {
	struct wlr_xdg_popup *popup, *tmp;
	wl_list_for_each_safe(popup, tmp, &view->wlr_xdg_toplevel->base->popups, link) {
		wlr_xdg_popup_destroy(popup);
	}
}

static void destroy(struct sway_view *view) {
	struct sway_xdg_shell_view *xdg_shell_view =
		xdg_shell_view_from_view(view);
	if (xdg_shell_view == NULL) {
		return;
	}
	free(xdg_shell_view);
}

static const struct sway_view_impl view_impl = {
	.get_constraints = get_constraints,
	.get_string_prop = get_string_prop,
	.configure = configure,
	.set_activated = set_activated,
	.set_tiled = set_tiled,
	.set_fullscreen = set_fullscreen,
	.set_resizing = set_resizing,
	.wants_floating = wants_floating,
	.is_transient_for = is_transient_for,
	.close = _close,
	.close_popups = close_popups,
	.destroy = destroy,
};

static bool view_wants_csd(struct sway_view *view) {
	struct wlr_xdg_toplevel *toplevel = view->wlr_xdg_toplevel;
	struct wlr_xdg_surface *xdg_surface = toplevel->base;
	if (view->xdg_decoration) {
		enum wlr_xdg_toplevel_decoration_v1_mode mode =
			view->xdg_decoration->wlr_xdg_decoration->requested_mode;
		return mode == WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
	}
	struct sway_server_decoration *deco =
			decoration_from_surface(xdg_surface->surface);
	return !deco || deco->wlr_server_decoration->mode ==
		WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT;
}

static void handle_commit(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_view *xdg_shell_view =
		wl_container_of(listener, xdg_shell_view, commit);
	struct sway_view *view = &xdg_shell_view->view;
	struct wlr_xdg_surface *xdg_surface = view->wlr_xdg_toplevel->base;

	if (xdg_surface->initial_commit) {
		if (view->xdg_decoration != NULL) {
			set_xdg_decoration_mode(view->xdg_decoration);
		}

		wlr_xdg_surface_schedule_configure(xdg_surface);
		wlr_xdg_toplevel_set_wm_capabilities(view->wlr_xdg_toplevel,
			XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN);
		view_setup(&xdg_shell_view->view, xdg_surface->surface, false, NULL,
			view_wants_csd(&xdg_shell_view->view));
		transaction_commit_dirty();

		// TODO: wlr_xdg_toplevel_set_bounds()
		return;
	}

	if (!xdg_surface->surface->mapped) {
		return;
	}

	struct wlr_box *new_geo = &xdg_surface->geometry;
	bool new_size = new_geo->width != view->geometry.width ||
			new_geo->height != view->geometry.height ||
			new_geo->x != view->geometry.x ||
			new_geo->y != view->geometry.y;

	if (new_size) {
		// The client changed its surface size in this commit. For floating
		// containers, we resize the container to match. For tiling containers,
		// we only recenter the surface.
		memcpy(&view->geometry, new_geo, sizeof(struct wlr_box));
		if (container_is_floating(view->container)) {
			view_update_size(view);
			// Only set the toplevel size the current container actually has a size.
			if (view->container->current.width) {
				wlr_xdg_toplevel_set_size(view->wlr_xdg_toplevel, view->geometry.width,
					view->geometry.height);
			}
			transaction_commit_dirty_client();
		}

		view_center_and_clip_surface(view);
	}

	if (view->container->node.instruction) {
		bool successful = transaction_notify_view_ready_by_serial(view,
				xdg_surface->current.configure_serial);

		// If we saved the view and this commit isn't what we're looking for
		// that means the user will never actually see the buffers submitted to
		// us here. Just send frame done events to these surfaces so they can
		// commit another time for us.
		if (view->saved_surface_tree && !successful) {
			view_send_frame_done(view);
		}
	}
}

static void handle_set_title(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_view *xdg_shell_view =
		wl_container_of(listener, xdg_shell_view, set_title);
	struct sway_view *view = &xdg_shell_view->view;
	view_update_title(view, false);
	view_execute_criteria(view);
}

static void handle_set_app_id(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_view *xdg_shell_view =
		wl_container_of(listener, xdg_shell_view, set_app_id);
	struct sway_view *view = &xdg_shell_view->view;
	view_update_app_id(view);
	view_execute_criteria(view);
}

static void handle_new_popup(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_view *xdg_shell_view =
		wl_container_of(listener, xdg_shell_view, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;

	struct sway_xdg_popup *popup = popup_create(wlr_popup,
		&xdg_shell_view->view, root->layers.popup);
	if (!popup) {
		return;
	}

	int lx, ly;
	wlr_scene_node_coords(&popup->view->content_tree->node, &lx, &ly);
	wlr_scene_node_set_position(&popup->scene_tree->node, lx, ly);
}

static void handle_request_maximize(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_view *xdg_shell_view =
		wl_container_of(listener, xdg_shell_view, request_maximize);
	struct wlr_xdg_toplevel *toplevel = xdg_shell_view->view.wlr_xdg_toplevel;
	wlr_xdg_surface_schedule_configure(toplevel->base);
}

static void handle_request_fullscreen(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_view *xdg_shell_view =
		wl_container_of(listener, xdg_shell_view, request_fullscreen);
	struct wlr_xdg_toplevel *toplevel = xdg_shell_view->view.wlr_xdg_toplevel;
	struct sway_view *view = &xdg_shell_view->view;

	if (!toplevel->base->surface->mapped) {
		return;
	}

	struct sway_container *container = view->container;
	struct wlr_xdg_toplevel_requested *req = &toplevel->requested;
	if (req->fullscreen && req->fullscreen_output && req->fullscreen_output->data) {
		struct sway_output *output = req->fullscreen_output->data;
		struct sway_workspace *ws = output_get_active_workspace(output);
		if (ws && !container_is_scratchpad_hidden(container) &&
				container->pending.workspace != ws) {
			if (container_is_floating(container)) {
				workspace_add_floating(ws, container);
			} else {
				container = workspace_add_tiling(ws, container);
			}
		}
	}

	container_set_fullscreen(container, req->fullscreen);

	arrange_root();
	transaction_commit_dirty();
}

static void handle_request_move(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_view *xdg_shell_view =
		wl_container_of(listener, xdg_shell_view, request_move);
	struct sway_view *view = &xdg_shell_view->view;
	if (!container_is_floating(view->container) ||
			view->container->pending.fullscreen_mode) {
		return;
	}
	struct wlr_xdg_toplevel_move_event *e = data;
	struct sway_seat *seat = e->seat->seat->data;
	if (e->serial == seat->last_button_serial) {
		seatop_begin_move_floating(seat, view->container);
	}
}

static void handle_request_resize(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_view *xdg_shell_view =
		wl_container_of(listener, xdg_shell_view, request_resize);
	struct sway_view *view = &xdg_shell_view->view;
	if (!container_is_floating(view->container)) {
		return;
	}
	struct wlr_xdg_toplevel_resize_event *e = data;
	struct sway_seat *seat = e->seat->seat->data;
	if (e->serial == seat->last_button_serial) {
		seatop_begin_resize_floating(seat, view->container, e->edges);
	}
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_view *xdg_shell_view =
		wl_container_of(listener, xdg_shell_view, unmap);
	struct sway_view *view = &xdg_shell_view->view;

	if (!sway_assert(view->surface, "Cannot unmap unmapped view")) {
		return;
	}

	view_unmap(view);

	wl_list_remove(&xdg_shell_view->new_popup.link);
	wl_list_remove(&xdg_shell_view->request_maximize.link);
	wl_list_remove(&xdg_shell_view->request_fullscreen.link);
	wl_list_remove(&xdg_shell_view->request_move.link);
	wl_list_remove(&xdg_shell_view->request_resize.link);
	wl_list_remove(&xdg_shell_view->set_title.link);
	wl_list_remove(&xdg_shell_view->set_app_id.link);
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_view *xdg_shell_view =
		wl_container_of(listener, xdg_shell_view, map);
	struct sway_view *view = &xdg_shell_view->view;
	struct wlr_xdg_toplevel *toplevel = view->wlr_xdg_toplevel;

	view->natural_width = toplevel->base->geometry.width;
	view->natural_height = toplevel->base->geometry.height;

	view_setup(view, toplevel->base->surface, false, NULL, view_wants_csd(view));
	view_map(view, toplevel->base->surface);

	transaction_commit_dirty();

	xdg_shell_view->new_popup.notify = handle_new_popup;
	wl_signal_add(&toplevel->base->events.new_popup,
		&xdg_shell_view->new_popup);

	xdg_shell_view->request_maximize.notify = handle_request_maximize;
	wl_signal_add(&toplevel->events.request_maximize,
			&xdg_shell_view->request_maximize);

	xdg_shell_view->request_fullscreen.notify = handle_request_fullscreen;
	wl_signal_add(&toplevel->events.request_fullscreen,
			&xdg_shell_view->request_fullscreen);

	xdg_shell_view->request_move.notify = handle_request_move;
	wl_signal_add(&toplevel->events.request_move,
			&xdg_shell_view->request_move);

	xdg_shell_view->request_resize.notify = handle_request_resize;
	wl_signal_add(&toplevel->events.request_resize,
			&xdg_shell_view->request_resize);

	xdg_shell_view->set_title.notify = handle_set_title;
	wl_signal_add(&toplevel->events.set_title,
			&xdg_shell_view->set_title);

	xdg_shell_view->set_app_id.notify = handle_set_app_id;
	wl_signal_add(&toplevel->events.set_app_id,
			&xdg_shell_view->set_app_id);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_xdg_shell_view *xdg_shell_view =
		wl_container_of(listener, xdg_shell_view, destroy);
	struct sway_view *view = &xdg_shell_view->view;
	if (!sway_assert(view->surface == NULL ||
			!view->surface->mapped, "Tried to destroy a mapped view")) {
		return;
	}
	wl_list_remove(&xdg_shell_view->destroy.link);
	wl_list_remove(&xdg_shell_view->map.link);
	wl_list_remove(&xdg_shell_view->unmap.link);
	wl_list_remove(&xdg_shell_view->commit.link);
	view->wlr_xdg_toplevel = NULL;
	if (view->xdg_decoration) {
		view->xdg_decoration->view = NULL;
	}
	view_begin_destroy(view);
}

struct sway_view *view_from_wlr_xdg_surface(
		struct wlr_xdg_surface *xdg_surface) {
	return xdg_surface->data;
}

void handle_xdg_shell_toplevel(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel *xdg_toplevel = data;

	sway_log(SWAY_DEBUG, "New xdg_shell toplevel title='%s' app_id='%s'",
		xdg_toplevel->title, xdg_toplevel->app_id);
	wlr_xdg_surface_ping(xdg_toplevel->base);

	struct sway_xdg_shell_view *xdg_shell_view =
		calloc(1, sizeof(struct sway_xdg_shell_view));
	if (!sway_assert(xdg_shell_view, "Failed to allocate view")) {
		return;
	}

	if (!view_init(&xdg_shell_view->view, SWAY_VIEW_XDG_SHELL, &view_impl)) {
		free(xdg_shell_view);
		return;
	}
	xdg_shell_view->view.wlr_xdg_toplevel = xdg_toplevel;

	xdg_shell_view->map.notify = handle_map;
	wl_signal_add(&xdg_toplevel->base->surface->events.map, &xdg_shell_view->map);

	xdg_shell_view->unmap.notify = handle_unmap;
	wl_signal_add(&xdg_toplevel->base->surface->events.unmap, &xdg_shell_view->unmap);

	xdg_shell_view->commit.notify = handle_commit;
	wl_signal_add(&xdg_toplevel->base->surface->events.commit,
		&xdg_shell_view->commit);

	xdg_shell_view->destroy.notify = handle_destroy;
	wl_signal_add(&xdg_toplevel->events.destroy, &xdg_shell_view->destroy);

	wlr_scene_xdg_surface_create(xdg_shell_view->view.content_tree, xdg_toplevel->base);

	xdg_toplevel->base->data = xdg_shell_view;
}
