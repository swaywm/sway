#ifndef _SWAY_EXTENSIONS_H
#define _SWAY_EXTENSIONS_H

#include <wayland-server.h>
#include <wlc/wlc-wayland.h>
#include "notification-area.h"
#include "wayland-desktop-shell-server-protocol.h"
#include "list.h"

struct background_config {
	wlc_handle output;
	wlc_resource surface;
	// we need the wl_resource of the surface in the destructor
	struct wl_resource *wl_surface_res;
	struct wl_client *client;
    wlc_handle handle;
};

struct panel_config {
	// wayland resource used in callbacks, is used to track this panel
	struct wl_resource *wl_resource;
	wlc_handle output;
	wlc_resource surface;
	// we need the wl_resource of the surface in the destructor
	struct wl_resource *wl_surface_res;
	enum desktop_shell_panel_position panel_position;
	// used to determine if client is a panel
	struct wl_client *client;
	// wlc handle for this panel's surface, not set until panel is created
	wlc_handle handle;
};

struct desktop_shell_state {
	list_t *backgrounds;
	list_t *panels;
	list_t *lock_surfaces;
	bool is_locked;
};

struct swaylock_state {
	bool active;
	wlc_handle output;
	wlc_resource surface;
};

extern struct desktop_shell_state desktop_shell;
extern struct wlc_notification_area *notification_area;

void register_extensions(void);

#endif
