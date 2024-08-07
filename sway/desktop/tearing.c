#include <wayland-server-core.h>
#include <wlr/types/wlr_tearing_control_v1.h>
#include "sway/server.h"
#include "sway/tree/view.h"
#include "sway/output.h"
#include "log.h"

struct sway_tearing_controller {
	struct wlr_tearing_control_v1 *tearing_control;
	struct wl_listener set_hint;
	struct wl_listener destroy;

	struct wl_list link; // sway_server::tearing_controllers
};

static void handle_tearing_controller_set_hint(struct wl_listener *listener,
		void *data) {
	struct sway_tearing_controller *controller =
		wl_container_of(listener, controller, set_hint);

	struct sway_view *view = view_from_wlr_surface(
		controller->tearing_control->surface);
	if (view) {
		view->tearing_hint = controller->tearing_control->current;
	}
}

static void handle_tearing_controller_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_tearing_controller *controller =
		wl_container_of(listener, controller, destroy);
	wl_list_remove(&controller->set_hint.link);
	wl_list_remove(&controller->destroy.link);
	wl_list_remove(&controller->link);
	free(controller);
}

void handle_new_tearing_hint(struct wl_listener *listener,
		void *data) {
	struct sway_server *server =
		wl_container_of(listener, server, tearing_control_new_object);
	struct wlr_tearing_control_v1 *tearing_control = data;

	enum wp_tearing_control_v1_presentation_hint hint =
		wlr_tearing_control_manager_v1_surface_hint_from_surface(
			server->tearing_control_v1, tearing_control->surface);
	sway_log(SWAY_DEBUG, "New presentation hint %d received for surface %p",
		hint, tearing_control->surface);

	struct sway_tearing_controller *controller =
		calloc(1, sizeof(struct sway_tearing_controller));
	if (!controller) {
		return;
	}

	controller->tearing_control = tearing_control;
	controller->set_hint.notify = handle_tearing_controller_set_hint;
	wl_signal_add(&tearing_control->events.set_hint, &controller->set_hint);
	controller->destroy.notify = handle_tearing_controller_destroy;
	wl_signal_add(&tearing_control->events.destroy, &controller->destroy);
	wl_list_init(&controller->link);

	wl_list_insert(&server->tearing_controllers, &controller->link);
}
