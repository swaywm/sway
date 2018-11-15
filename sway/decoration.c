#include <stdlib.h>
#include "sway/decoration.h"
#include "sway/desktop/transaction.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"
#include "log.h"

static void server_decoration_handle_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_server_decoration *deco =
		wl_container_of(listener, deco, destroy);
	wl_list_remove(&deco->destroy.link);
	wl_list_remove(&deco->mode.link);
	wl_list_remove(&deco->link);
	free(deco);
}

static void server_decoration_handle_mode(struct wl_listener *listener,
		void *data) {
	struct sway_server_decoration *deco =
		wl_container_of(listener, deco, mode);
	struct sway_view *view =
		view_from_wlr_surface(deco->wlr_server_decoration->surface);
	if (view == NULL || view->surface != deco->wlr_server_decoration->surface) {
		return;
	}

	bool csd = deco->wlr_server_decoration->mode ==
			WLR_SERVER_DECORATION_MANAGER_MODE_CLIENT;
	view_update_csd_from_client(view, csd);

	arrange_container(view->container);
	transaction_commit_dirty();
}

void handle_server_decoration(struct wl_listener *listener, void *data) {
	struct wlr_server_decoration *wlr_deco = data;

	struct sway_server_decoration *deco = calloc(1, sizeof(*deco));
	if (deco == NULL) {
		return;
	}

	deco->wlr_server_decoration = wlr_deco;

	wl_signal_add(&wlr_deco->events.destroy, &deco->destroy);
	deco->destroy.notify = server_decoration_handle_destroy;

	wl_signal_add(&wlr_deco->events.mode, &deco->mode);
	deco->mode.notify = server_decoration_handle_mode;

	wl_list_insert(&server.decorations, &deco->link);
}

struct sway_server_decoration *decoration_from_surface(
		struct wlr_surface *surface) {
	struct sway_server_decoration *deco;
	wl_list_for_each(deco, &server.decorations, link) {
		if (deco->wlr_server_decoration->surface == surface) {
			return deco;
		}
	}
	return NULL;
}
