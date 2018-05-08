#define _POSIX_C_SOURCE 200112L
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_gamma_control.h>
#include <wlr/types/wlr_linux_dmabuf.h>
#include <wlr/types/wlr_layer_shell.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_screenshooter.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_output.h>
#include <wlr/types/wlr_wl_shell.h>
#include <wlr/util/log.h>
// TODO WLR: make Xwayland optional
#include <wlr/xwayland.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/server.h"
#include "sway/tree/layout.h"

static void server_ready(struct wl_listener *listener, void *data) {
	wlr_log(L_DEBUG, "Compositor is ready, executing cmds in queue");
	// Execute commands until there are none left
	config->active = true;
	while (config->cmd_queue->length) {
		char *line = config->cmd_queue->items[0];
		struct cmd_results *res = execute_command(line, NULL);
		if (res->status != CMD_SUCCESS) {
			wlr_log(L_ERROR, "Error on line '%s': %s", line, res->error);
		}
		free_cmd_results(res);
		free(line);
		list_del(config->cmd_queue, 0);
	}
}

bool server_init(struct sway_server *server) {
	wlr_log(L_DEBUG, "Initializing Wayland server");

	server->wl_display = wl_display_create();
	server->wl_event_loop = wl_display_get_event_loop(server->wl_display);
	server->backend = wlr_backend_autocreate(server->wl_display);

	struct wlr_renderer *renderer = wlr_backend_get_renderer(server->backend);
	assert(renderer);

	wlr_renderer_init_wl_shm(renderer, server->wl_display);

	server->compositor = wlr_compositor_create(server->wl_display, renderer);
	server->data_device_manager =
		wlr_data_device_manager_create(server->wl_display);

	wlr_screenshooter_create(server->wl_display);
	wlr_gamma_control_manager_create(server->wl_display);
	wlr_primary_selection_device_manager_create(server->wl_display);

	server->new_output.notify = handle_new_output;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

	wlr_xdg_output_manager_create(server->wl_display,
			root_container.sway_root->output_layout);

	server->layer_shell = wlr_layer_shell_create(server->wl_display);
	wl_signal_add(&server->layer_shell->events.new_surface,
		&server->layer_shell_surface);
	server->layer_shell_surface.notify = handle_layer_shell_surface;

	server->xdg_shell_v6 = wlr_xdg_shell_v6_create(server->wl_display);
	wl_signal_add(&server->xdg_shell_v6->events.new_surface,
		&server->xdg_shell_v6_surface);
	server->xdg_shell_v6_surface.notify = handle_xdg_shell_v6_surface;

	server->wl_shell = wlr_wl_shell_create(server->wl_display);
	wl_signal_add(&server->wl_shell->events.new_surface,
		&server->wl_shell_surface);
	server->wl_shell_surface.notify = handle_wl_shell_surface;

	// TODO make xwayland optional
	server->xwayland =
		wlr_xwayland_create(server->wl_display, server->compositor, false);
	wl_signal_add(&server->xwayland->events.new_surface,
		&server->xwayland_surface);
	server->xwayland_surface.notify = handle_xwayland_surface;
	wl_signal_add(&server->xwayland->events.ready,
		&server->xwayland_ready);
	// TODO: call server_ready now if xwayland is not enabled
	server->xwayland_ready.notify = server_ready;

	// TODO: configurable cursor theme and size
	server->xcursor_manager = wlr_xcursor_manager_create(NULL, 24);
	wlr_xcursor_manager_load(server->xcursor_manager, 1);
	struct wlr_xcursor *xcursor = wlr_xcursor_manager_get_xcursor(
		server->xcursor_manager, "left_ptr", 1);
	if (xcursor != NULL) {
		struct wlr_xcursor_image *image = xcursor->images[0];
		wlr_xwayland_set_cursor(server->xwayland, image->buffer,
			image->width * 4, image->width, image->height, image->hotspot_x,
			image->hotspot_y);
	}

	// TODO: Integration with sway borders
	struct wlr_server_decoration_manager *deco_manager =
		wlr_server_decoration_manager_create(server->wl_display);
	wlr_server_decoration_manager_set_default_mode(
		deco_manager, WLR_SERVER_DECORATION_MANAGER_MODE_SERVER);

	wlr_linux_dmabuf_create(server->wl_display, renderer);

	server->socket = wl_display_add_socket_auto(server->wl_display);
	if (!server->socket) {
		wlr_log(L_ERROR, "Unable to open wayland socket");
		wlr_backend_destroy(server->backend);
		return false;
	}

	input_manager = input_manager_create(server);
	return true;
}

void server_fini(struct sway_server *server) {
	// TODO: free sway-specific resources
	wl_display_destroy(server->wl_display);
}

void server_run(struct sway_server *server) {
	wlr_log(L_INFO, "Running compositor on wayland display '%s'",
			server->socket);
	setenv("WAYLAND_DISPLAY", server->socket, true);
	if (!wlr_backend_start(server->backend)) {
		wlr_log(L_ERROR, "Failed to start backend");
		wlr_backend_destroy(server->backend);
		return;
	}
	wl_display_run(server->wl_display);
}
