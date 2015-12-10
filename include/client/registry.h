#ifndef _SWAY_CLIENT_REGISTRY_H
#define _SWAY_CLIENT_REGISTRY_H

#include <wayland-client.h>
#include "wayland-desktop-shell-client-protocol.h"
#include "wayland-swaylock-client-protocol.h"
#include "list.h"

struct output_state {
        struct wl_output *output;
        uint32_t flags;
        uint32_t width, height;
};

struct registry {
        struct wl_compositor *compositor;
        struct wl_display *display;
        struct wl_pointer *pointer;
        struct wl_keyboard *keyboard;
        struct wl_seat *seat;
        struct wl_shell *shell;
        struct wl_shm *shm;
        struct desktop_shell *desktop_shell;
        struct lock *swaylock;
        list_t *outputs;
};

struct registry *registry_poll(void);
void registry_teardown(struct registry *registry);

#endif
