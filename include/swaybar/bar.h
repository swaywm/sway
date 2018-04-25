#ifndef _SWAYBAR_BAR_H
#define _SWAYBAR_BAR_H
#include <wayland-client.h>
#include "pool-buffer.h"

struct swaybar_config;
struct swaybar_output;
struct swaybar_workspace;

struct swaybar_pointer {
	struct wl_pointer *pointer;
	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor_image *cursor_image;
	struct wl_surface *cursor_surface;
	struct swaybar_output *current;
	int x, y;
};

struct swaybar_hotspot {
	struct wl_list link;
	int x, y, width, height;
	void (*callback)(struct swaybar_output *output,
			int x, int y, uint32_t button, void *data);
	void (*destroy)(void *data);
	void *data;
};

struct swaybar {
	struct wl_display *display;
	struct wl_compositor *compositor;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_shm *shm;
	struct wl_seat *seat;

	struct swaybar_config *config;
	struct swaybar_output *focused_output;
	struct swaybar_pointer pointer;
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
	uint32_t wl_name;

	struct wl_list workspaces;
	struct wl_list hotspots;

	char *name;
	size_t index;
	bool focused;

	uint32_t width, height;
	int32_t scale;
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

void bar_setup(struct swaybar *bar,
	const char *socket_path,
	const char *bar_id);
void bar_run(struct swaybar *bar);
void bar_teardown(struct swaybar *bar);

void free_workspaces(struct wl_list *list);

#endif
