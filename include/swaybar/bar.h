#ifndef _SWAYBAR_BAR_H
#define _SWAYBAR_BAR_H
#include <wayland-client.h>
#include "input.h"
#include "pool-buffer.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

struct swaybar_config;
struct swaybar_output;
struct swaybar_workspace;
struct loop;

struct swaybar {
	char *id;
	char *mode;
	bool mode_pango_markup;

	// only relevant when bar is in "hide" mode
	bool visible_by_modifier;
	bool visible_by_urgency;
	bool visible;

	struct wl_display *display;
	struct wl_compositor *compositor;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct zxdg_output_manager_v1 *xdg_output_manager;
	struct wl_shm *shm;
	struct wl_seat *seat;

	struct swaybar_config *config;
	struct swaybar_pointer pointer;
	struct status_line *status;

	struct loop *eventloop;

	int ipc_event_socketfd;
	int ipc_socketfd;

	struct wl_list outputs; // swaybar_output::link
};

struct swaybar_output {
	struct wl_list link; // swaybar::outputs
	struct swaybar *bar;
	struct wl_output *output;
	struct zxdg_output_v1 *xdg_output;
	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	uint32_t wl_name;

	struct wl_list workspaces; // swaybar_workspace::link
	struct wl_list hotspots; // swaybar_hotspot::link

	char *name;
	bool focused;

	uint32_t width, height;
	int32_t scale;
	enum wl_output_subpixel subpixel;
	struct pool_buffer buffers[2];
	struct pool_buffer *current_buffer;
	bool dirty;
	bool frame_scheduled;
};

struct swaybar_workspace {
	struct wl_list link; // swaybar_output::workspaces
	int num;
	char *name;
	char *label;
	bool focused;
	bool visible;
	bool urgent;
};

bool bar_setup(struct swaybar *bar, const char *socket_path);
void bar_run(struct swaybar *bar);
void bar_teardown(struct swaybar *bar);

/*
 * Determines whether the bar should be visible and changes it to be so.
 * If the current visibility of the bar is the different to what it should be,
 * then it adds or destroys the layer surface as required,
 * as well as sending the cont or stop signal to the status command.
 * If the current visibility of the bar is already what it should be,
 * then this function is a no-op, unless moving_layer is true, which occurs
 * when the bar changes from "hide" to "dock" mode or vice versa, and the bar
 * needs to be destroyed and re-added in order to change its layer.
 *
 * Returns true if the bar is now visible, otherwise false.
 */
bool determine_bar_visibility(struct swaybar *bar, bool moving_layer);
void free_workspaces(struct wl_list *list);

#endif
