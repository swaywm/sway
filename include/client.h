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
    int width, height;
};

struct buffer {
    struct wl_buffer *buffer;
    struct wl_shm_pool *pool;
    uint32_t width, height;
};

struct client_state {
    struct wl_compositor *compositor;
    struct wl_display *display;
    struct wl_pointer *pointer;
    struct wl_seat *seat;
    struct wl_shell *shell;
    struct wl_shm *shm;
    struct buffer *buffer;
    struct wl_surface *surface;
    struct wl_shell_surface *shell_surface;
    struct wl_callback *frame_cb;
    bool busy;
    cairo_t *cairo;
    cairo_surface_t *cairo_surface;
    PangoContext *pango;
    list_t *outputs;
};

struct client_state *client_setup(void);
void client_teardown(struct client_state *state);
struct buffer *create_memory_pool(struct client_state *state, int32_t width, int32_t height, uint32_t format);
int client_prerender(struct client_state *state);
int client_render(struct client_state *state);

#endif
