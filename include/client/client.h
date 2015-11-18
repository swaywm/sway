#ifndef _CLIENT_H
#define _CLIENT_H

#include <wayland-client.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdbool.h>
#include "list.h"

struct output_state {
        struct wl_output *output;
        uint32_t flags;
        uint32_t width, height;
};

struct buffer {
        struct wl_buffer *buffer;
        int fd;
        cairo_surface_t *surface;
        cairo_t *cairo;
        PangoContext *pango;
        uint32_t width, height;
        bool busy;
};

struct client_state {
        struct wl_compositor *compositor;
        struct wl_display *display;
        struct wl_pointer *pointer;
        struct wl_seat *seat;
        struct wl_shell *shell;
        struct wl_shm *shm;
        struct buffer buffers[2];
        struct buffer *buffer;
        struct wl_surface *surface;
        struct wl_shell_surface *shell_surface;
        struct wl_callback *frame_cb;
        uint32_t width, height;
        cairo_t *cairo;
        list_t *outputs;
};

struct client_state *client_setup(uint32_t width, uint32_t height);
void client_teardown(struct client_state *state);
int client_prerender(struct client_state *state);
int client_render(struct client_state *state);

#endif
