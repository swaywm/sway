#include <stdlib.h>
#include <wlr/types/wlr_idle.h>
#include "log.h"
#include "sway/desktop/idle_inhibit_v1.h"
#include "sway/server.h"


static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_idle_inhibitor_v1 *inhibitor =
		wl_container_of(listener, inhibitor, destroy);
	wlr_log(L_DEBUG, "Sway idle inhibitor destroyed");
	wlr_idle_set_enabled(inhibitor->server->idle, NULL, true);
	wl_list_remove(&inhibitor->destroy.link);
	free(inhibitor);
}

void handle_idle_inhibitor_v1(struct wl_listener *listener, void *data) {
	struct wlr_idle_inhibitor_v1 *wlr_inhibitor = data;
	struct sway_server *server =
		wl_container_of(listener, server, new_idle_inhibitor_v1);
	wlr_log(L_DEBUG, "New sway idle inhibitor");

	struct sway_idle_inhibitor_v1 *inhibitor =
		calloc(1, sizeof(struct sway_idle_inhibitor_v1));
	if (!inhibitor) {
		return;
	}

	inhibitor->server = server;

	inhibitor->destroy.notify = handle_destroy;
	wl_signal_add(&wlr_inhibitor->events.destroy, &inhibitor->destroy);

	wlr_idle_set_enabled(server->idle, NULL, false);
}
