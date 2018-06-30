#include <stdlib.h>
#include <wlr/types/wlr_idle.h>
#include "log.h"
#include "sway/desktop/idle_inhibit_v1.h"
#include "sway/tree/view.h"
#include "sway/server.h"


static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_idle_inhibitor_v1 *inhibitor =
		wl_container_of(listener, inhibitor, destroy);
	wlr_log(L_DEBUG, "Sway idle inhibitor destroyed");
	wlr_idle_set_enabled(inhibitor->server->idle, NULL, true);
	wl_list_remove(&inhibitor->link);
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
	inhibitor->view = view_from_wlr_surface(wlr_inhibitor->surface);
	wl_list_insert(&server->idle_inhibitors_v1, &inhibitor->link);


	inhibitor->destroy.notify = handle_destroy;
	wl_signal_add(&wlr_inhibitor->events.destroy, &inhibitor->destroy);

	wlr_idle_set_enabled(server->idle, NULL, false);
}

void idle_inhibit_v1_check_active(struct sway_server *server) {
	struct sway_idle_inhibitor_v1 *inhibitor;
	bool inhibited = false;
	wl_list_for_each(inhibitor, &server->idle_inhibitors_v1, link) {
		if (!inhibitor->view) {
			/* Cannot guess if view is visible so assume it is */
			inhibited = true;
			break;
		}
		if (view_is_visible(inhibitor->view)) {
			inhibited = true;
			break;
		}
	}
	wlr_idle_set_enabled(server->idle, NULL, !inhibited);
}
