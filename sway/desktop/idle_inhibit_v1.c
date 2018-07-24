#include <stdlib.h>
#include <wlr/types/wlr_idle.h>
#include "log.h"
#include "sway/desktop/idle_inhibit_v1.h"
#include "sway/tree/view.h"
#include "sway/server.h"


static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_idle_inhibitor_v1 *inhibitor =
		wl_container_of(listener, inhibitor, destroy);
	wlr_log(WLR_DEBUG, "Sway idle inhibitor destroyed");
	wl_list_remove(&inhibitor->link);
	wl_list_remove(&inhibitor->destroy.link);
	idle_inhibit_v1_check_active(inhibitor->manager);
	free(inhibitor);
}

void handle_idle_inhibitor_v1(struct wl_listener *listener, void *data) {
	struct wlr_idle_inhibitor_v1 *wlr_inhibitor = data;
	struct sway_idle_inhibit_manager_v1 *manager =
		wl_container_of(listener, manager, new_idle_inhibitor_v1);
	wlr_log(WLR_DEBUG, "New sway idle inhibitor");

	struct sway_idle_inhibitor_v1 *inhibitor =
		calloc(1, sizeof(struct sway_idle_inhibitor_v1));
	if (!inhibitor) {
		return;
	}

	inhibitor->manager = manager;
	inhibitor->view = view_from_wlr_surface(wlr_inhibitor->surface);
	wl_list_insert(&manager->inhibitors, &inhibitor->link);


	inhibitor->destroy.notify = handle_destroy;
	wl_signal_add(&wlr_inhibitor->events.destroy, &inhibitor->destroy);

	idle_inhibit_v1_check_active(manager);
}

void idle_inhibit_v1_check_active(
		struct sway_idle_inhibit_manager_v1 *manager) {
	struct sway_idle_inhibitor_v1 *inhibitor;
	bool inhibited = false;
	wl_list_for_each(inhibitor, &manager->inhibitors, link) {
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
	wlr_idle_set_enabled(manager->idle, NULL, !inhibited);
}

struct sway_idle_inhibit_manager_v1 *sway_idle_inhibit_manager_v1_create(
		struct wl_display *wl_display, struct wlr_idle *idle) {
	struct sway_idle_inhibit_manager_v1 *manager =
		calloc(1, sizeof(struct sway_idle_inhibit_manager_v1));
	if (!manager) {
		return NULL;
	}

	manager->wlr_manager = wlr_idle_inhibit_v1_create(wl_display);
	if (!manager->wlr_manager) {
		free(manager);
		return NULL;
	}
	manager->idle = idle;
	wl_signal_add(&manager->wlr_manager->events.new_inhibitor,
		&manager->new_idle_inhibitor_v1);
	manager->new_idle_inhibitor_v1.notify = handle_idle_inhibitor_v1;
	wl_list_init(&manager->inhibitors);

	return manager;
}
