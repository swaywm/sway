#ifndef _SWAY_OUTPUT_H
#define _SWAY_OUTPUT_H
#include <time.h>
#include <wayland-server.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_output.h>
#include <unistd.h>

struct sway_server;
struct sway_container;

struct sway_output {
	struct wlr_output *wlr_output;
	struct sway_container *swayc;
	struct sway_server *server;
	struct timespec last_frame;

	struct wl_list layers[4]; // sway_layer_surface::link
	struct wlr_box usable_area;

	struct wl_listener frame;
	struct wl_listener destroy;
	struct wl_listener mode;

	pid_t bg_pid;
};

#endif
