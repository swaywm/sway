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
	deco->view->xdg_decoration = NULL;
	wl_list_remove(&deco->destroy.link);
	wl_list_remove(&deco->surface_commit.link);
	wl_list_remove(&deco->link);
	free(deco);
}

static void xdg_decoration_handle_surface_commit(struct wl_listener *listener,
		void *data) {
	struct sway_xdg_decoration *decoration =
		wl_container_of(listener, decoration, surface_commit);

	bool csd = decoration->wlr_xdg_decoration->current_mode ==
		WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
	struct sway_view *view = decoration->view;

	view_update_csd_from_client(view, csd);

	arrange_container(view->container);
	transaction_commit_dirty();
}

void handle_xdg_decoration(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_decoration_v1 *wlr_deco = data;
	struct sway_xdg_shell_view *xdg_shell_view = wlr_deco->surface->data;
	struct wlr_xdg_surface *wlr_xdg_surface =
		xdg_shell_view->view.wlr_xdg_surface;

	struct sway_xdg_decoration *deco = calloc(1, sizeof(*deco));
	if (deco == NULL) {
		return;
	}

	deco->view = &xdg_shell_view->view;
	deco->view->xdg_decoration = deco;
	deco->wlr_xdg_decoration = wlr_deco;

	wl_signal_add(&wlr_deco->events.destroy, &deco->destroy);
	deco->destroy.notify = xdg_decoration_handle_destroy;

	// Note: We don't listen to the request_mode signal here, effectively
	// ignoring any modes the client asks to set. The client can still force a
	// mode upon us, in which case we get upset but live with it.

	deco->surface_commit.notify = xdg_decoration_handle_surface_commit;
	wl_signal_add(&wlr_xdg_surface->surface->events.commit,
		&deco->surface_commit);

	wl_list_insert(&server.xdg_decorations, &deco->link);
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
