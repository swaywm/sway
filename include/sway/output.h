#ifndef _SWAY_OUTPUT_H
#define _SWAY_OUTPUT_H
#include <time.h>
#include <wayland-server.h>
#include <wlr/types/wlr_output.h>

struct sway_server;
struct sway_container;

struct sway_output {
	struct wlr_output *wlr_output;
	struct sway_container *swayc;
	struct sway_server *server;
	struct timespec last_frame;
	struct wl_listener frame;
	struct wl_listener resolution;
};

#endif
