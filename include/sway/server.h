#ifndef _SWAY_SERVER_H
#define _SWAY_SERVER_H
#include <stdbool.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_data_device_manager.h>
#include <wlr/render.h>
// TODO WLR: make Xwayland optional
#include <wlr/xwayland.h>

struct sway_server {
	// TODO WLR
	//struct roots_desktop *desktop;
	//struct roots_input *input;

	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;

	struct wlr_backend *backend;
	struct wlr_renderer *renderer;

	struct wlr_data_device_manager *data_device_manager;
};

bool server_init(struct sway_server *server);
void server_fini(struct sway_server *server);

struct sway_server server;

#endif
