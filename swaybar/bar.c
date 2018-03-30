#define _XOPEN_SOURCE 500
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wlr/util/log.h>
#include "swaybar/render.h"
#include "swaybar/config.h"
#include "swaybar/event_loop.h"
#include "swaybar/status_line.h"
#include "swaybar/bar.h"
#include "swaybar/ipc.h"
#include "ipc-client.h"
#include "list.h"
#include "pango.h"
#include "pool-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static void bar_init(struct swaybar *bar) {
	bar->config = init_config();
	wl_list_init(&bar->outputs);
}

struct swaybar_output *new_output(const char *name) {
	struct swaybar_output *output = malloc(sizeof(struct swaybar_output));
	output->name = strdup(name);
	return output;
}

static void layer_surface_configure(void *data,
		struct zwlr_layer_surface_v1 *surface,
		uint32_t serial, uint32_t width, uint32_t height) {
	struct swaybar_output *output = data;
	output->width = width;
	output->height = height;
	zwlr_layer_surface_v1_ack_configure(surface, serial);
	render_frame(output->bar, output);
}

static void layer_surface_closed(void *_output,
		struct zwlr_layer_surface_v1 *surface) {
	// TODO: Deal with hotplugging
	struct swaybar_output *output = _output;
	zwlr_layer_surface_v1_destroy(output->layer_surface);
	wl_surface_destroy(output->surface);
}

struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = layer_surface_configure,
	.closed = layer_surface_closed,
};

static void handle_global(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct swaybar *bar = data;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		bar->compositor = wl_registry_bind(registry, name,
				&wl_compositor_interface, 1);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		bar->shm = wl_registry_bind(registry, name,
				&wl_shm_interface, 1);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		static size_t index = 0;
		struct swaybar_output *output =
			calloc(1, sizeof(struct swaybar_output));
		output->bar = bar;
		output->output = wl_registry_bind(registry, name,
				&wl_output_interface, 1);
		output->index = index++;
		wl_list_init(&output->workspaces);
		wl_list_insert(&bar->outputs, &output->link);
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		bar->layer_shell = wl_registry_bind(
				registry, name, &zwlr_layer_shell_v1_interface, 1);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry,
		uint32_t name) {
	// who cares
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

void bar_setup(struct swaybar *bar,
		const char *socket_path, const char *bar_id) {
	bar_init(bar);
	init_event_loop();

	bar->ipc_socketfd = ipc_open_socket(socket_path);
	bar->ipc_event_socketfd = ipc_open_socket(socket_path);
	ipc_initialize(bar, bar_id);
	if (bar->config->status_command) {
		bar->status = status_line_init(bar->config->status_command);
	}

	assert(bar->display = wl_display_connect(NULL));

	struct wl_registry *registry = wl_display_get_registry(bar->display);
	wl_registry_add_listener(registry, &registry_listener, bar);
	wl_display_roundtrip(bar->display);
	assert(bar->compositor && bar->layer_shell && bar->shm);

	// TODO: we might not necessarily be meant to do all of the outputs
	struct swaybar_output *output;
	wl_list_for_each(output, &bar->outputs, link) {
		struct config_output *coutput;
		wl_list_for_each(coutput, &bar->config->outputs, link) {
			if (coutput->index != output->index) {
				continue;
			}
			output->name = strdup(coutput->name);
			assert(output->surface = wl_compositor_create_surface(
					bar->compositor));
			output->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
					bar->layer_shell, output->surface, output->output,
					ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, "panel");
			assert(output->layer_surface);
			zwlr_layer_surface_v1_add_listener(output->layer_surface,
					&layer_surface_listener, output);
			zwlr_layer_surface_v1_set_anchor(output->layer_surface,
					bar->config->position);
			break;
		}
	}
	ipc_get_workspaces(bar);
	wl_list_for_each(output, &bar->outputs, link) {
		render_frame(bar, output);
	}
}

static void render_all_frames(struct swaybar *bar) {
	struct swaybar_output *output;
	wl_list_for_each(output, &bar->outputs, link) {
		render_frame(bar, output);
	}
}

static void display_in(int fd, short mask, void *_bar) {
	struct swaybar *bar = (struct swaybar *)_bar;
	if (wl_display_dispatch(bar->display) == -1) {
		bar_teardown(bar);
		exit(0);
	}
}

static void ipc_in(int fd, short mask, void *_bar) {
	struct swaybar *bar = (struct swaybar *)_bar;
	if (handle_ipc_event(bar)) {
		render_all_frames(bar);
	}
}

static void status_in(int fd, short mask, void *_bar) {
	struct swaybar *bar = (struct swaybar *)_bar;
	if (handle_status_readable(bar->status)) {
		render_all_frames(bar);
	}
}

void bar_run(struct swaybar *bar) {
	add_event(wl_display_get_fd(bar->display), POLLIN, display_in, bar);
	add_event(bar->ipc_event_socketfd, POLLIN, ipc_in, bar);
	if (bar->status) {
		add_event(bar->status->read_fd, POLLIN, status_in, bar);
	}
	while (1) {
		event_loop_poll();
	}
}

static void free_outputs(struct wl_list *list) {
	struct swaybar_output *output, *tmp;
	wl_list_for_each_safe(output, tmp, list, link) {
		wl_list_remove(&output->link);
		free(output->name);
		free(output);
	}
}

void bar_teardown(struct swaybar *bar) {
	free_outputs(&bar->outputs);
	if (bar->config) {
		free_config(bar->config);
	}
	close(bar->ipc_event_socketfd);
	close(bar->ipc_socketfd);
	if (bar->status) {
		status_line_free(bar->status);
	}
}
