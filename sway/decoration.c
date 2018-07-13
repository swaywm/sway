#include <stdlib.h>
#include "sway/decoration.h"
#include "sway/server.h"
#include "sway/tree/view.h"
#include "log.h"

static void server_decoration_handle_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_server_decoration *deco =
		wl_container_of(listener, deco, destroy);
	wl_list_remove(&deco->destroy.link);
	free(deco);
}

static void server_decoration_handle_mode(struct wl_listener *listener,
		void *data) {
	struct sway_server_decoration *deco =
		wl_container_of(listener, deco, mode);
	struct sway_view *view =
		view_from_wlr_surface(deco->wlr_server_decoration->surface);

	// TODO
	wlr_log(WLR_ERROR, "%p %d", view, deco->wlr_server_decoration->mode);
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
}
