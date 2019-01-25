#ifndef _SWAY_SERVER_H
#define _SWAY_SERVER_H
#include <stdbool.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/types/wlr_xdg_shell.h>
#include "config.h"
#include "list.h"
#if HAVE_XWAYLAND
#include "sway/xwayland.h"
#endif

struct sway_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;
	const char *socket;

	struct wlr_backend *backend;
	struct wlr_backend *noop_backend;

	struct wlr_compositor *compositor;
	struct wlr_data_device_manager *data_device_manager;

	struct sway_input_manager *input;

	struct wl_listener new_output;

	struct wlr_idle *idle;
	struct sway_idle_inhibit_manager_v1 *idle_inhibit_manager_v1;

	struct wlr_layer_shell_v1 *layer_shell;
	struct wl_listener layer_shell_surface;

	struct wlr_xdg_shell_v6 *xdg_shell_v6;
	struct wl_listener xdg_shell_v6_surface;

	struct wlr_xdg_shell *xdg_shell;
	struct wl_listener xdg_shell_surface;

#if HAVE_XWAYLAND
	struct sway_xwayland xwayland;
	struct wl_listener xwayland_surface;
	struct wl_listener xwayland_ready;
#endif

	struct wlr_server_decoration_manager *server_decoration_manager;
	struct wl_listener server_decoration;
	struct wl_list decorations; // sway_server_decoration::link

	struct wlr_xdg_decoration_manager_v1 *xdg_decoration_manager;
	struct wl_listener xdg_decoration;
	struct wl_list xdg_decorations; // sway_xdg_decoration::link

	struct wlr_presentation *presentation;

	struct wlr_pointer_constraints_v1 *pointer_constraints;
	struct wl_listener pointer_constraint;

	size_t txn_timeout_ms;
	list_t *transactions;
	list_t *dirty_nodes;
};

struct sway_server server;

/* Prepares an unprivileged server_init by performing all privileged operations in advance */
bool server_privileged_prepare(struct sway_server *server);
bool server_init(struct sway_server *server);
void server_fini(struct sway_server *server);
bool server_start(struct sway_server *server);
void server_run(struct sway_server *server);

void handle_new_output(struct wl_listener *listener, void *data);

void handle_idle_inhibitor_v1(struct wl_listener *listener, void *data);
void handle_layer_shell_surface(struct wl_listener *listener, void *data);
void handle_xdg_shell_v6_surface(struct wl_listener *listener, void *data);
void handle_xdg_shell_surface(struct wl_listener *listener, void *data);
#if HAVE_XWAYLAND
void handle_xwayland_surface(struct wl_listener *listener, void *data);
#endif
void handle_server_decoration(struct wl_listener *listener, void *data);
void handle_xdg_decoration(struct wl_listener *listener, void *data);
void handle_pointer_constraint(struct wl_listener *listener, void *data);

#endif
