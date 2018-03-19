#ifndef _SWAY_SERVER_H
#define _SWAY_SERVER_H
#include <stdbool.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/render/wlr_renderer.h>
// TODO WLR: make Xwayland optional
#include <wlr/xwayland.h>

struct sway_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;
	const char *socket;

	struct wlr_backend *backend;
	struct wlr_renderer *renderer;

	struct wlr_compositor *compositor;
	struct wlr_data_device_manager *data_device_manager;

	struct sway_input_manager *input;

	struct wl_listener new_output;
	struct wl_listener output_frame;

	struct wlr_xdg_shell_v6 *xdg_shell_v6;
	struct wl_listener xdg_shell_v6_surface;

	struct wlr_xwayland *xwayland;
	struct wl_listener xwayland_surface;
	struct wl_listener xwayland_ready;

	struct wlr_wl_shell *wl_shell;
	struct wl_listener wl_shell_surface;
};

struct sway_server server;

bool server_init(struct sway_server *server);
void server_fini(struct sway_server *server);
void server_run(struct sway_server *server);

void handle_new_output(struct wl_listener *listener, void *data);

void handle_xdg_shell_v6_surface(struct wl_listener *listener, void *data);
void handle_xwayland_surface(struct wl_listener *listener, void *data);
void handle_wl_shell_surface(struct wl_listener *listener, void *data);

#endif
