#define _POSIX_C_SOURCE 200112L
#include <strings.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/backend/multi.h>
#include <wlr/render.h>
#include <wlr/render/gles2.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_wl_shell.h>
// TODO WLR: make Xwayland optional
#include <wlr/xwayland.h>
#include "sway/server.h"
#include "sway/input/input-manager.h"
#include "log.h"

bool server_init(struct sway_server *server) {
	sway_log(L_DEBUG, "Initializing Wayland server");

	server->wl_display = wl_display_create();
	server->wl_event_loop = wl_display_get_event_loop(server->wl_display);
	server->backend = wlr_backend_autocreate(server->wl_display);
	wl_list_init(&server->subbackends);

	server->renderer = wlr_gles2_renderer_create(server->backend);
	wl_display_init_shm(server->wl_display);

	server->compositor = wlr_compositor_create(
			server->wl_display, server->renderer);

	server->data_device_manager =
		wlr_data_device_manager_create(server->wl_display);

	server->output_add.notify = output_add_notify;
	wl_signal_add(&server->backend->events.output_add, &server->output_add);

	server->output_remove.notify = output_remove_notify;
	wl_signal_add(&server->backend->events.output_remove,
			&server->output_remove);

	server->xdg_shell_v6 = wlr_xdg_shell_v6_create(server->wl_display);
	wl_signal_add(&server->xdg_shell_v6->events.new_surface,
		&server->xdg_shell_v6_surface);
	server->xdg_shell_v6_surface.notify = handle_xdg_shell_v6_surface;

	// TODO make xwayland optional
	server->xwayland =
		wlr_xwayland_create(server->wl_display, server->compositor);
	wl_signal_add(&server->xwayland->events.new_surface,
		&server->xwayland_surface);
	server->xwayland_surface.notify = handle_xwayland_surface;

	server->wl_shell = wlr_wl_shell_create(server->wl_display);
	wl_signal_add(&server->wl_shell->events.new_surface,
		&server->wl_shell_surface);
	server->wl_shell_surface.notify = handle_wl_shell_surface;

	server->socket = wl_display_add_socket_auto(server->wl_display);
	if (!sway_assert(server->socket,  "Unable to open wayland socket")) {
		wlr_backend_destroy(server->backend);
		return false;
	}

	input_manager = sway_input_manager_create(server);

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

static void sway_subbackend_destroy(struct sway_subbackend *subbackend) {
	wl_list_remove(&subbackend->backend_destroy.link);
	wl_list_remove(&subbackend->link);
	// free(name)?
	free(subbackend);
}

struct sway_subbackend *sway_subbackend_create(enum sway_subbackend_type type,
		char *name) {
	struct sway_subbackend *subbackend =
		calloc(1, sizeof(struct sway_subbackend));
	if (subbackend == NULL) {
		sway_log(L_ERROR, "could not allocate subbackend");
		return NULL;
	}

	if (name == NULL) {
		// TODO: figure out what to call the backend based on what the backend
		// type is and how many other backends are configured of that type
		// (<type>-<num>).
	} else {
		subbackend->name = name;
	}

	subbackend->type = type;
	wl_list_init(&subbackend->link);

	return subbackend;
}

static void handle_subbackend_backend_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_subbackend *subbackend =
		wl_container_of(listener, subbackend, backend_destroy);
	sway_subbackend_destroy(subbackend);
}

static struct wlr_backend *wayland_backend_create() {
	sway_log(L_DEBUG, "TODO: create wayland backend");
	return NULL;
}

static struct wlr_backend *x11_backend_create() {
	sway_log(L_DEBUG, "TODO: create x11 backend");
	return NULL;
}

static struct wlr_backend *headless_backend_create() {
	sway_log(L_DEBUG, "TODO: create headless backend");
	return NULL;
}

static struct wlr_backend *drm_backend_create() {
	sway_log(L_DEBUG, "TODO: create drm backend");
	return NULL;
}

void sway_server_add_subbackend(struct sway_server *server,
		struct sway_subbackend *subbackend) {
	struct wlr_backend *backend = NULL;

	switch (subbackend->type) {
	case SWAY_SUBBACKEND_WAYLAND:
		backend = wayland_backend_create();
		break;
	case SWAY_SUBBACKEND_X11:
		backend = x11_backend_create();
		break;
	case SWAY_SUBBACKEND_DRM:
		backend = drm_backend_create();
		break;
	case SWAY_SUBBACKEND_HEADLESS:
		backend = headless_backend_create();
		break;
	}

	if (backend == NULL) {
		sway_log(L_ERROR, "could not create subbackend '%s'", subbackend->name);
		sway_subbackend_destroy(subbackend);
		return;
	}

	wl_list_remove(&subbackend->link);
	wl_list_insert(&server->subbackends, &subbackend->link);

	wl_signal_add(&backend->events.destroy, &subbackend->backend_destroy);
	subbackend->backend_destroy.notify = handle_subbackend_backend_destroy;

	wlr_multi_backend_add(server->backend, backend);
}

void sway_server_remove_subbackend(struct sway_server *server, char *name) {
	struct sway_subbackend *subbackend = NULL;
	wl_list_for_each(subbackend, &server->subbackends, link) {
		if (strcasecmp(subbackend->name, name) == 0) {
			break;
		}
	}

	if (!subbackend) {
		sway_log(L_DEBUG, "could not find subbackend named '%s'", name);
		return;
	}

	wlr_backend_destroy(subbackend->backend);
}
