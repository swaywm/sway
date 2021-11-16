#include <stdlib.h>
#include "sway/desktop/transaction.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"
#include "sway/xdg_decoration.h"
#include "log.h"

static void xdg_decoration_handle_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_xdg_decoration *deco =
		wl_container_of(listener, deco, destroy);
	if (deco->view) {
		deco->view->xdg_decoration = NULL;
	}
	wl_list_remove(&deco->destroy.link);
	wl_list_remove(&deco->request_mode.link);
	wl_list_remove(&deco->link);
	free(deco);
}

static void xdg_decoration_handle_request_mode(struct wl_listener *listener,
		void *data) {
	struct sway_xdg_decoration *deco =
		wl_container_of(listener, deco, request_mode);
	struct sway_view *view = deco->view;
	enum wlr_xdg_toplevel_decoration_v1_mode mode =
		WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE;
	enum wlr_xdg_toplevel_decoration_v1_mode client_mode =
		deco->wlr_xdg_decoration->requested_mode;

	bool floating;
	if (view->container) {
		floating = container_is_floating(view->container);
		bool csd = false;
		csd = client_mode ==
			WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
		view_update_csd_from_client(view, csd);
		arrange_container(view->container);
		transaction_commit_dirty();
	} else {
		floating = view->impl->wants_floating &&
			view->impl->wants_floating(view);
	}

	if (floating && client_mode) {
		mode = client_mode;
	}

	wlr_xdg_toplevel_decoration_v1_set_mode(deco->wlr_xdg_decoration,
			mode);
}

void handle_xdg_decoration(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_decoration_v1 *wlr_deco = data;
	struct sway_xdg_shell_view *xdg_shell_view = wlr_deco->surface->data;

	struct sway_xdg_decoration *deco = calloc(1, sizeof(*deco));
	if (deco == NULL) {
		return;
	}

	deco->view = &xdg_shell_view->view;
	deco->view->xdg_decoration = deco;
	deco->wlr_xdg_decoration = wlr_deco;

	wl_signal_add(&wlr_deco->events.destroy, &deco->destroy);
	deco->destroy.notify = xdg_decoration_handle_destroy;

	wl_signal_add(&wlr_deco->events.request_mode, &deco->request_mode);
	deco->request_mode.notify = xdg_decoration_handle_request_mode;

	wl_list_insert(&server.xdg_decorations, &deco->link);

	xdg_decoration_handle_request_mode(&deco->request_mode, wlr_deco);
}

struct sway_xdg_decoration *xdg_decoration_from_surface(
		struct wlr_surface *surface) {
	struct sway_xdg_decoration *deco;
	wl_list_for_each(deco, &server.xdg_decorations, link) {
		if (deco->wlr_xdg_decoration->surface->surface == surface) {
			return deco;
		}
	}
	return NULL;
}
