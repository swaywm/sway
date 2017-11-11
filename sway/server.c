#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <stdbool.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_data_device_manager.h>
#include <wlr/render.h>
#include <wlr/render/gles2.h>
// TODO WLR: make Xwayland optional
#include <wlr/xwayland.h>
#include "sway/server.h"
#include "sway/input.h"
#include "log.h"

bool server_init(struct sway_server *server) {
	sway_log(L_DEBUG, "Initializing Wayland server");

	server->wl_display = wl_display_create();
	server->wl_event_loop = wl_display_get_event_loop(server->wl_display);
	server->backend = wlr_backend_autocreate(server->wl_display);

	server->renderer = wlr_gles2_renderer_create(server->backend);
	wl_display_init_shm(server->wl_display);

	server->input = sway_input_create(server);
	server->data_device_manager =
		wlr_data_device_manager_create(server->wl_display);

	server->output_add.notify = output_add_notify;
	wl_signal_add(&server->backend->events.output_add, &server->output_add);

	server->output_remove.notify = output_remove_notify;
	wl_signal_add(&server->backend->events.output_remove,
			&server->output_remove);

	server->socket = wl_display_add_socket_auto(server->wl_display);
	if (!sway_assert(server->socket,  "Unable to open wayland socket")) {
		wlr_backend_destroy(server->backend);
		return false;
	}
	return true;
}

void server_fini(struct sway_server *server) {
	// TODO WLR: tear down more stuff
	wlr_backend_destroy(server->backend);
}

void server_run(struct sway_server *server) {
	sway_log(L_INFO, "Running compositor on wayland display '%s'",
			server->socket);
	setenv("_WAYLAND_DISPLAY", server->socket, true);
	if (!sway_assert(wlr_backend_start(server->backend),
				"Failed to start backend")) {
		wlr_backend_destroy(server->backend);
		return;
	}
	setenv("WAYLAND_DISPLAY", server->socket, true);
	wl_display_run(server->wl_display);
}
