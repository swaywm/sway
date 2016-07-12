#ifndef _CLIENT_H
#define _CLIENT_H

#include <wayland-client.h>
#include "wayland-desktop-shell-client-protocol.h"
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdbool.h>
#include "list.h"
#include "client/registry.h"

struct window;

struct buffer {
	struct wl_buffer *buffer;
	cairo_surface_t *surface;
	cairo_t *cairo;
	PangoContext *pango;
	uint32_t width, height;
	bool busy;
};

struct cursor {
	struct wl_surface *surface;
	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor *cursor;
	struct wl_poitner *pointer;
};

struct pointer_input {
	int last_x;
	int last_y;

	void (*notify_button)(struct window *window, int x, int y, uint32_t button);
};

struct window {
	struct registry *registry;
	struct buffer buffers[2];
	struct buffer *buffer;
	struct wl_surface *surface;
	struct wl_shell_surface *shell_surface;
	struct wl_callback *frame_cb;
	struct cursor cursor;
	uint32_t width, height;
	char *font;
	cairo_t *cairo;
	struct pointer_input pointer_input;
};

struct window *window_setup(struct registry *registry, uint32_t width, uint32_t height, bool shell_surface);
void window_teardown(struct window *state);
int window_prerender(struct window *state);
int window_render(struct window *state);
void window_make_shell(struct window *window);

#endif
