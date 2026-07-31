#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_xdg_system_bell_v1.h>
#include "list.h"
#include "log.h"
#include "sway/config.h"
#include "sway/criteria.h"
#include "sway/desktop/transaction.h"
#include "sway/input/seat.h"
#include "sway/server.h"
#include "sway/tree/view.h"
#include "sway/commands.h"

static void execute_bell_bindings(struct sway_view *view) {
	struct sway_seat *seat = input_manager_current_seat();
	struct sway_container *con = view ? view->container : NULL;

	list_t *bindings = config->current_mode->bell_bindings;
	for (int i = 0; i < bindings->length; ++i) {
		struct sway_bell_binding *binding = bindings->items[i];

		if (binding->criteria &&
				(!view || !criteria_matches_view(binding->criteria, view))) {
			continue;
		}

		list_t *res_list = execute_command(binding->command, seat, con);
		if (!res_list) {
			continue;
		}
		for (int j = 0; j < res_list->length; ++j) {
			struct cmd_results *results = res_list->items[j];
			if (results->status != CMD_SUCCESS) {
				sway_log(SWAY_ERROR,
						"could not run command for bell binding: %s (%s)",
						binding->command, results->error);
			}
			free_cmd_results(results);
		}
		list_free(res_list);
	}

	transaction_commit_dirty();
}

static void handle_xdg_system_bell_v1_ring(struct wl_listener *listener,
		void *data) {
	struct wlr_xdg_system_bell_v1_ring_event *event = data;

	sway_log(SWAY_DEBUG, "xdg_system_bell_v1: ring event received");

	struct sway_view *view = NULL;
	if (event->surface) {
		view = view_from_wlr_surface(event->surface);
	}

	execute_bell_bindings(view);
}

bool sway_xdg_system_bell_v1_init(void) {
	if (!server.xdg_system_bell_v1) {
		sway_log(SWAY_DEBUG, "xdg_system_bell_v1 not available");
		return false;
	}

	server.xdg_system_bell_v1_ring.notify = handle_xdg_system_bell_v1_ring;
	wl_signal_add(&server.xdg_system_bell_v1->events.ring,
			&server.xdg_system_bell_v1_ring);

	sway_log(SWAY_DEBUG, "xdg_system_bell_v1 initialized");
	return true;
}
