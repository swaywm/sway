#define _POSIX_C_SOURCE 200112L
#include <strings.h>
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

	wl_list_init(&server->subbackends);

	if (headless) {
		server->backend = wlr_multi_backend_create(server->wl_display);
		struct sway_subbackend *subbackend =
			sway_subbackend_create(SWAY_SUBBACKEND_HEADLESS, "headless");
		sway_server_add_subbackend(server, subbackend);
	} else {
		// TODO add whatever this function creates to the subbackends
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

static void sway_subbackend_destroy(struct sway_subbackend *subbackend) {
	if (subbackend->backend) {
		wl_list_remove(&subbackend->backend_destroy.link);
	}
	wl_list_remove(&subbackend->link);
	// free(name)?
	free(subbackend);
}

struct sway_subbackend *sway_server_get_subbackend(struct sway_server *server,
		char *name) {
	struct sway_subbackend *subbackend = NULL;
	wl_list_for_each(subbackend, &server->subbackends, link) {
		if (strcasecmp(subbackend->name, name) == 0) {
			return subbackend;
		}
	}

	return NULL;
}

struct sway_subbackend *sway_subbackend_create(enum sway_subbackend_type type,
		char *name) {
	struct sway_subbackend *subbackend =
		calloc(1, sizeof(struct sway_subbackend));
	if (subbackend == NULL) {
		wlr_log(L_ERROR, "could not allocate subbackend");
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
	wl_list_init(&subbackend->outputs);
	wl_list_init(&subbackend->inputs);
	wl_list_init(&subbackend->link);

	return subbackend;
}

static void handle_subbackend_backend_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_subbackend *subbackend =
		wl_container_of(listener, subbackend, backend_destroy);
	sway_subbackend_destroy(subbackend);
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

void sway_server_add_subbackend(struct sway_server *server,
		struct sway_subbackend *subbackend) {
	if (sway_server_get_subbackend(server, subbackend->name)) {
		wlr_log(L_ERROR, "cannot add subbackend '%s': already exists",
			subbackend->name);
		sway_subbackend_destroy(subbackend);
		return;
	}

	struct wlr_backend *backend = NULL;

	switch (subbackend->type) {
	case SWAY_SUBBACKEND_WAYLAND:
		backend = wayland_backend_create(server);
		break;
	case SWAY_SUBBACKEND_X11:
		backend = x11_backend_create(server);
		break;
	case SWAY_SUBBACKEND_DRM:
		backend = drm_backend_create(server);
		break;
	case SWAY_SUBBACKEND_HEADLESS:
		backend = headless_backend_create(server);
		break;
	}

	if (backend == NULL) {
		wlr_log(L_ERROR, "could not create subbackend '%s'", subbackend->name);
		sway_subbackend_destroy(subbackend);
		return;
	}

	wl_list_remove(&subbackend->link);
	wl_list_insert(&server->subbackends, &subbackend->link);

	wl_signal_add(&backend->events.destroy, &subbackend->backend_destroy);
	subbackend->backend_destroy.notify = handle_subbackend_backend_destroy;

	wlr_multi_backend_add(server->backend, backend);
	wlr_backend_start(backend);
}

void sway_server_remove_subbackend(struct sway_server *server, char *name) {
	struct sway_subbackend *subbackend =
		sway_server_get_subbackend(server, name);

	if (!subbackend) {
		wlr_log(L_DEBUG, "could not find subbackend named '%s'", name);
		return;
	}

	wlr_backend_destroy(subbackend->backend);
}

struct subbackend_output {
	struct sway_subbackend *backend;
	struct wlr_output *wlr_output;

	struct wl_listener output_destroy;

	struct wl_list link; // sway_subbackend::outputs
};

static void subbackend_output_destroy(struct subbackend_output *output) {
	wl_list_remove(&output->link);
	wl_list_remove(&output->output_destroy.link);
	free(output);
}

static void handle_subbackend_output_destroy(struct wl_listener *listener,
		void *data) {
	struct subbackend_output *output =
		wl_container_of(listener, output, output_destroy);
	subbackend_output_destroy(output);
}

void sway_subbackend_add_output(struct sway_server *server,
		struct sway_subbackend *subbackend, char *name) {
	struct wlr_output *wlr_output = NULL;

	switch(subbackend->type) {
	case SWAY_SUBBACKEND_WAYLAND:
		wlr_log(L_DEBUG, "TODO: create wayland subbackend output");
		break;
	case SWAY_SUBBACKEND_X11:
		wlr_log(L_DEBUG, "TODO: create x11 subbackend output");
		break;
	case SWAY_SUBBACKEND_DRM:
		wlr_log(L_DEBUG, "creating DRM subbackend outputs is not supported");
		break;
	case SWAY_SUBBACKEND_HEADLESS:
		// TODO allow to name the output
		wlr_output =
			wlr_headless_add_output(subbackend->backend, 500, 500);
		break;
	}

	if (wlr_output == NULL) {
		wlr_log(L_ERROR, "could not create subbackend output '%s'", name);
		return;
	}

	struct subbackend_output *output =
		calloc(1, sizeof(struct subbackend_output));
	if (output == NULL) {
		wlr_log(L_ERROR, "could not allocate subbackend output");
		return;
	}

	output->wlr_output = wlr_output;

	wl_signal_add(&wlr_output->events.destroy, &output->output_destroy);
	output->output_destroy.notify = handle_subbackend_output_destroy;

	wl_list_insert(&subbackend->outputs, &output->link);
}

void sway_subbackend_remove_output(struct sway_server *server,
		struct sway_subbackend *subbackend, char *name) {
	struct subbackend_output *output = NULL, *tmp = NULL;
	wl_list_for_each_safe(output, tmp, &subbackend->outputs, link) {
		if (strcasecmp(output->wlr_output->name, name) == 0) {
			wlr_output_destroy(output->wlr_output);
		}
	}
}

struct subbackend_input {
	struct sway_subbackend *backend;
	struct wlr_input_device *device;

	struct wl_listener input_destroy;

	struct wl_list link; // sway_subbackend::inputs
};

static void subbackend_input_destroy(struct subbackend_input *input) {
	wl_list_remove(&input->link);
	wl_list_remove(&input->input_destroy.link);
	free(input);
}

static void handle_subbackend_device_destroy(struct wl_listener *listener,
		void *data) {
	struct subbackend_input *input =
		wl_container_of(listener, input, input_destroy);
	subbackend_input_destroy(input);
}

void sway_subbackend_add_input(struct sway_server *server,
		struct sway_subbackend *subbackend, enum wlr_input_device_type type,
		char *name) {
	if (subbackend->type != SWAY_SUBBACKEND_HEADLESS) {
		wlr_log(L_DEBUG, "adding inputs is only supported for the headless backend");
		return;
	}

	// TODO allow naming the input device
	struct wlr_input_device *device =
		wlr_headless_add_input_device(subbackend->backend, type);
	if (device == NULL) {
		return;
	}

	struct subbackend_input *input =
		calloc(1, sizeof(struct subbackend_input));
	if (input == NULL) {
		wlr_log(L_ERROR, "could not allocate subbackend input device");
		return;
	}

	input->device = device;

	wl_signal_add(&device->events.destroy, &input->input_destroy);
	input->input_destroy.notify = handle_subbackend_device_destroy;

	wl_list_insert(&subbackend->inputs, &input->link);
}

void sway_subbackend_remove_input(struct sway_server *server,
		struct sway_subbackend *subbackend, char *name) {
	struct subbackend_input *input = NULL, *tmp = NULL;
	wl_list_for_each_safe(input, tmp, &subbackend->inputs, link) {
		if (strcasecmp(input->device->name, name) == 0) {
			wlr_input_device_destroy(input->device);
		}
	}
}
