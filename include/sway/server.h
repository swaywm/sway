#ifndef _SWAY_SERVER_H
#define _SWAY_SERVER_H
#include <stdbool.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_layer_shell.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/render/wlr_renderer.h>
// TODO WLR: make Xwayland optional
#include "list.h"
#include "sway/xwayland.h"

struct sway_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;
	const char *socket;

	struct wlr_backend *backend;

	struct wlr_compositor *compositor;
	struct wlr_data_device_manager *data_device_manager;

	struct sway_input_manager *input;

	struct wl_listener new_output;

	struct wlr_idle *idle;
	struct sway_idle_inhibit_manager_v1 *idle_inhibit_manager_v1;

	struct wlr_layer_shell *layer_shell;
	struct wl_listener layer_shell_surface;

	struct wlr_xdg_shell_v6 *xdg_shell_v6;
	struct wl_listener xdg_shell_v6_surface;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener xdg_shell_surface;

	struct sway_xwayland xwayland;
	struct wl_listener xwayland_surface;
	struct wl_listener xwayland_ready;

	bool debug_txn_timings;

	list_t *transactions;
	list_t *dirty_containers;

	list_t *scratchpad; // struct sway_container
};

struct sway_server server;

/* Prepares an unprivileged server_init by performing all privileged operations in advance */
bool server_privileged_prepare(struct sway_server *server);
bool server_init(struct sway_server *server);
void server_fini(struct sway_server *server);
bool server_start_backend(struct sway_server *server);
void server_run(struct sway_server *server);

void handle_new_output(struct wl_listener *listener, void *data);

void handle_idle_inhibitor_v1(struct wl_listener *listener, void *data);
void handle_layer_shell_surface(struct wl_listener *listener, void *data);
void handle_xdg_shell_v6_surface(struct wl_listener *listener, void *data);
void handle_xdg_shell_surface(struct wl_listener *listener, void *data);
void handle_xwayland_surface(struct wl_listener *listener, void *data);

#endif
