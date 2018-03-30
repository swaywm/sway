#ifndef _SWAYBAR_BAR_H
#define _SWAYBAR_BAR_H
#include <wayland-client.h>
#include "pool-buffer.h"
#include "list.h"

struct swaybar_config;
struct swaybar_output;
struct swaybar_workspace;

struct swaybar {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_shm *shm;

	struct swaybar_config *config;
	struct swaybar_output *focused_output;
	struct status_line *status;

	int ipc_event_socketfd;
	int ipc_socketfd;

	struct wl_list outputs;
};

struct swaybar_output {
	struct wl_list link;
	struct swaybar *bar;
	struct wl_output *output;
	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;

	struct wl_list workspaces;

	char *name;
	size_t index;
	bool focused;

	uint32_t width, height;
	struct pool_buffer buffers[2];
	struct pool_buffer *current_buffer;
};

struct swaybar_workspace {
	struct wl_list link;
	int num;
	char *name;
	bool focused;
	bool visible;
	bool urgent;
};

// TODO: Rename stuff to match wlroots conventions (init/create/etc)
void bar_setup(struct swaybar *bar,
	const char *socket_path,
	const char *bar_id);
void bar_run(struct swaybar *bar);
void bar_teardown(struct swaybar *bar);

#endif
