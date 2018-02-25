#ifndef _SWAY_SERVER_H
#define _SWAY_SERVER_H
#include <stdbool.h>
#include <wayland-server.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/render.h>
// TODO WLR: make Xwayland optional
#include <wlr/xwayland.h>

enum sway_subbackend_type {
	SWAY_SUBBACKEND_WAYLAND,
	SWAY_SUBBACKEND_X11,
	SWAY_SUBBACKEND_DRM,
	SWAY_SUBBACKEND_HEADLESS,
};

struct sway_subbackend {
	char *name;
	enum sway_subbackend_type type;
	struct wlr_backend *backend;

	struct wl_list outputs;
	struct wl_list inputs;

	struct wl_listener backend_destroy;

	struct wl_list link; // sway_server::subbackends

};

struct sway_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;
	const char *socket;

	struct wlr_backend *backend;
	struct wl_list subbackends; // sway_server_subbackend::link
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

bool server_init(struct sway_server *server, bool headless);
void server_fini(struct sway_server *server);
void server_run(struct sway_server *server);

void handle_new_output(struct wl_listener *listener, void *data);

void handle_xdg_shell_v6_surface(struct wl_listener *listener, void *data);
void handle_xwayland_surface(struct wl_listener *listener, void *data);
void handle_wl_shell_surface(struct wl_listener *listener, void *data);

struct sway_subbackend *sway_subbackend_create(enum sway_subbackend_type type,
		char *name);
void sway_server_add_subbackend(struct sway_server *server,
		struct sway_subbackend *subbackend);
void sway_server_remove_subbackend(struct sway_server *server, char *name);
struct sway_subbackend *sway_server_get_subbackend(struct sway_server *server,
		char *name);

void sway_subbackend_add_output(struct sway_server *server,
		struct sway_subbackend *subbackend, char *name);
void sway_subbackend_remove_output(struct sway_server *server,
		struct sway_subbackend *subbackend, char *name);

void sway_subbackend_add_input(struct sway_server *server,
		struct sway_subbackend *subbackend, enum wlr_input_device_type type,
		char *name);
void sway_subbackend_remove_input(struct sway_server *server,
		struct sway_subbackend *subbackend, char *name);

#endif
