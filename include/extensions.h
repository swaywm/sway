#ifndef _SWAY_EXTENSIONS_H
#define _SWAY_EXTENSIONS_H

#include <wayland-server.h>
#include <wlc/wlc-wayland.h>
#include "wayland-desktop-shell-server-protocol.h"
#include "list.h"

struct background_config {
        wlc_handle output;
        wlc_resource surface;
        struct wl_resource *resource;
};

struct panel_config {
        wlc_handle output;
        wlc_resource surface;
        struct wl_resource *resource;
};

struct desktop_shell_state {
        list_t *backgrounds;
        list_t *panels;
        list_t *lock_surfaces;
        bool is_locked;
        enum desktop_shell_panel_position panel_position;
        struct wlc_size panel_size;
};

struct swaylock_state {
        bool active;
        wlc_handle output;
        wlc_resource surface;
};

extern struct desktop_shell_state desktop_shell;

void register_extensions(void);

#endif
