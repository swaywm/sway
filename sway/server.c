#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 700
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#include <wlr/backend/headless.h>
#include <wlr/render.h>
#include <wlr/render/gles2.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_wl_shell.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/interfaces/wlr_input_device.h>
// TODO WLR: make Xwayland optional
#include <wlr/xwayland.h>
#include <wlr/util/log.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/input/input-manager.h"
#include "log.h"

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

bool server_init(struct sway_server *server, bool headless) {
	wlr_log(L_DEBUG, "Initializing Wayland server");

	server->wl_display = wl_display_create();
	server->wl_event_loop = wl_display_get_event_loop(server->wl_display);

	wl_list_init(&server->backends);

	if (headless) {
		server->backend = wlr_multi_backend_create(server->wl_display);
		struct sway_backend *backend =
			sway_backend_create(SWAY_BACKEND_HEADLESS, "headless");
		sway_server_add_backend(server, backend);
		sway_backend_add_output(server, backend, "headless");
	} else {
		// TODO add whatever this function creates to the backends
		server->backend = wlr_backend_autocreate(server->wl_display);
	}

	wl_display_init_shm(server->wl_display);

	server->renderer = wlr_backend_get_renderer(server->backend);

	server->compositor = wlr_compositor_create(
			server->wl_display, server->renderer);

	server->data_device_manager =
		wlr_data_device_manager_create(server->wl_display);

	server->new_output.notify = handle_new_output;
	wl_signal_add(&server->backend->events.new_output, &server->new_output);

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
	wl_signal_add(&server->xwayland->events.ready,
		&server->xwayland_ready);
	// TODO: call server_ready now if xwayland is not enabled
	server->xwayland_ready.notify = server_ready;

	server->wl_shell = wlr_wl_shell_create(server->wl_display);
	wl_signal_add(&server->wl_shell->events.new_surface,
		&server->wl_shell_surface);
	server->wl_shell_surface.notify = handle_wl_shell_surface;

	server->socket = wl_display_add_socket_auto(server->wl_display);
	if (!server->socket) {
		wlr_log(L_ERROR, "Unable to open wayland socket");
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
	wlr_log(L_INFO, "Running compositor on wayland display '%s'",
			server->socket);
	setenv("_WAYLAND_DISPLAY", server->socket, true);
	if (!wlr_backend_start(server->backend)) {
		wlr_log(L_ERROR, "Failed to start backend");
		wlr_backend_destroy(server->backend);
		return;
	}
	setenv("WAYLAND_DISPLAY", server->socket, true);
	wl_display_run(server->wl_display);
}

static void sway_backend_destroy(struct sway_backend *backend) {
	if (backend->backend) {
		wl_list_remove(&backend->backend_destroy.link);
	}
	wl_list_remove(&backend->link);
	// free(name)?
	free(backend);
}

struct sway_backend *sway_server_get_backend(struct sway_server *server,
		char *name) {
	struct sway_backend *backend = NULL;
	wl_list_for_each(backend, &server->backends, link) {
		if (strcasecmp(backend->name, name) == 0) {
			return backend;
		}
	}

	return NULL;
}

struct sway_backend *sway_backend_create(enum sway_backend_type type,
		char *name) {
	struct sway_backend *backend =
		calloc(1, sizeof(struct sway_backend));
	if (backend == NULL) {
		wlr_log(L_ERROR, "could not allocate backend");
		return NULL;
	}

	if (name == NULL) {
		// TODO: figure out what to call the backend based on what the backend
		// type is and how many other backends are configured of that type
		// (<type>-<num>).
	} else {
		backend->name = name;
	}

	backend->type = type;
	wl_list_init(&backend->outputs);
	wl_list_init(&backend->inputs);
	wl_list_init(&backend->link);

	return backend;
}

static void handle_backend_backend_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_backend *backend =
		wl_container_of(listener, backend, backend_destroy);
	sway_backend_destroy(backend);
}

static struct wlr_backend *wayland_backend_create(struct sway_server *server) {
	wlr_log(L_DEBUG, "TODO: create wayland backend");
	return NULL;
	/*
	struct wlr_backend *backend =
		wlr_wl_backend_create(server->wl_display);
	wlr_wl_output_create(backend);
	return backend;
	*/
}

static struct wlr_backend *x11_backend_create(struct sway_server *server) {
	wlr_log(L_DEBUG, "TODO: create x11 backend");
	return NULL;
}

static struct wlr_backend *headless_backend_create(
		struct sway_server *server) {
	struct wlr_backend *backend =
		wlr_headless_backend_create(server->wl_display);
	return backend;
}

static struct wlr_backend *drm_backend_create(struct sway_server *server) {
	wlr_log(L_DEBUG, "TODO: create drm backend");
	return NULL;
}

void sway_server_add_backend(struct sway_server *server,
		struct sway_backend *backend) {
	if (sway_server_get_backend(server, backend->name)) {
		wlr_log(L_ERROR, "cannot add backend '%s': already exists",
			backend->name);
		sway_backend_destroy(backend);
		return;
	}

	struct wlr_backend *wlr_backend = NULL;

	switch (backend->type) {
	case SWAY_BACKEND_WAYLAND:
		wlr_backend = wayland_backend_create(server);
		break;
	case SWAY_BACKEND_X11:
		wlr_backend = x11_backend_create(server);
		break;
	case SWAY_BACKEND_DRM:
		wlr_backend = drm_backend_create(server);
		break;
	case SWAY_BACKEND_HEADLESS:
		wlr_backend = headless_backend_create(server);
		break;
	}

	if (backend == NULL) {
		wlr_log(L_ERROR, "could not create backend '%s'", backend->name);
		sway_backend_destroy(backend);
		return;
	}

	backend->backend = wlr_backend;

	wl_list_remove(&backend->link);
	wl_list_insert(&server->backends, &backend->link);

	wl_signal_add(&wlr_backend->events.destroy, &backend->backend_destroy);
	backend->backend_destroy.notify = handle_backend_backend_destroy;

	wlr_multi_backend_add(server->backend, wlr_backend);
	wlr_backend_start(wlr_backend);
}

void sway_server_remove_backend(struct sway_server *server, char *name) {
	struct sway_backend *backend =
		sway_server_get_backend(server, name);

	if (!backend) {
		wlr_log(L_DEBUG, "could not find backend named '%s'", name);
		return;
	}

	wlr_backend_destroy(backend->backend);
}

struct backend_output {
	struct sway_backend *backend;
	struct wlr_output *wlr_output;

	struct wl_listener output_destroy;

	struct wl_list link; // sway_backend::outputs
};

static void backend_output_destroy(struct backend_output *output) {
	wl_list_remove(&output->link);
	wl_list_remove(&output->output_destroy.link);
	free(output);
}

static void handle_backend_output_destroy(struct wl_listener *listener,
		void *data) {
	struct backend_output *output =
		wl_container_of(listener, output, output_destroy);
	backend_output_destroy(output);
}

void sway_backend_add_output(struct sway_server *server,
		struct sway_backend *backend, char *name) {
	struct wlr_output *wlr_output = NULL;

	switch(backend->type) {
	case SWAY_BACKEND_WAYLAND:
		wlr_log(L_DEBUG, "TODO: create wayland backend output");
		break;
	case SWAY_BACKEND_X11:
		wlr_log(L_DEBUG, "TODO: create x11 backend output");
		break;
	case SWAY_BACKEND_DRM:
		wlr_log(L_DEBUG, "creating DRM backend outputs is not supported");
		break;
	case SWAY_BACKEND_HEADLESS:
		wlr_output =
			wlr_headless_add_output(backend->backend, 1280, 960);
		break;
	}

	if (wlr_output == NULL) {
		wlr_log(L_ERROR, "could not create backend output '%s'", name);
		return;
	}

	struct backend_output *output =
		calloc(1, sizeof(struct backend_output));
	if (output == NULL) {
		wlr_output_destroy(wlr_output);
		wlr_log(L_ERROR, "could not allocate backend output");
		return;
	}

	strncpy(wlr_output->name, name, sizeof(wlr_output->name));
	for (int i = 0; i < root_container.children->length; ++i) {
		swayc_t *output = root_container.children->items[i];
		if (output->type != C_OUTPUT) {
			continue;
		}

		if (output->sway_output->wlr_output == wlr_output) {
			free(output->name);
			output->name = strdup(name);
			break;
		}
	}

	output->wlr_output = wlr_output;

	wl_signal_add(&wlr_output->events.destroy, &output->output_destroy);
	output->output_destroy.notify = handle_backend_output_destroy;

	wl_list_insert(&backend->outputs, &output->link);
}

void sway_backend_remove_output(struct sway_server *server,
		struct sway_backend *backend, char *name) {
	struct backend_output *output = NULL, *tmp = NULL;
	wl_list_for_each_safe(output, tmp, &backend->outputs, link) {
		if (strcasecmp(output->wlr_output->name, name) == 0) {
			wlr_output_destroy(output->wlr_output);
		}
	}
}

struct backend_input {
	struct sway_backend *backend;
	struct wlr_input_device *device;

	struct wl_listener input_destroy;

	struct wl_list link; // sway_backend::inputs
};

static void backend_input_destroy(struct backend_input *input) {
	wl_list_remove(&input->link);
	wl_list_remove(&input->input_destroy.link);
	free(input);
}

static void handle_backend_device_destroy(struct wl_listener *listener,
		void *data) {
	struct backend_input *input =
		wl_container_of(listener, input, input_destroy);
	backend_input_destroy(input);
}

void sway_backend_add_input(struct sway_server *server,
		struct sway_backend *backend, enum wlr_input_device_type type,
		char *name) {
	if (backend->type != SWAY_BACKEND_HEADLESS) {
		wlr_log(L_DEBUG, "adding inputs is only supported for the headless backend");
		return;
	}

	// TODO allow naming the input device
	struct wlr_input_device *device =
		wlr_headless_add_input_device(backend->backend, type);
	if (device == NULL) {
		return;
	}

	struct backend_input *input =
		calloc(1, sizeof(struct backend_input));
	if (input == NULL) {
		wlr_log(L_ERROR, "could not allocate backend input device");
		return;
	}

	input->device = device;

	wl_signal_add(&device->events.destroy, &input->input_destroy);
	input->input_destroy.notify = handle_backend_device_destroy;

	wl_list_insert(&backend->inputs, &input->link);
}

void sway_backend_remove_input(struct sway_server *server,
		struct sway_backend *backend, char *name) {
	struct backend_input *input = NULL, *tmp = NULL;
	wl_list_for_each_safe(input, tmp, &backend->inputs, link) {
		if (strcasecmp(input->device->name, name) == 0) {
			wlr_input_device_destroy(input->device);
		}
	}
}
