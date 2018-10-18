#ifndef _SWAYBAR_INPUT_H
#define _SWAYBAR_INPUT_H

#include <wayland-client.h>
#include "list.h"

struct swaybar_output;

struct swaybar_pointer {
	struct wl_pointer *pointer;
	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor_image *cursor_image;
	struct wl_surface *cursor_surface;
	struct swaybar_output *current;
	int x, y;
};

enum x11_button {
	NONE,
	LEFT,
	MIDDLE,
	RIGHT,
	SCROLL_UP,
	SCROLL_DOWN,
	SCROLL_LEFT,
	SCROLL_RIGHT,
	BACK,
	FORWARD,
};

enum hotspot_event_handling {
	HOTSPOT_IGNORE,
	HOTSPOT_PROCESS,
};

struct swaybar_hotspot {
	struct wl_list link; // swaybar_output::hotspots
	int x, y, width, height;
	enum hotspot_event_handling (*callback)(struct swaybar_output *output,
			int x, int y, enum x11_button button, void *data);
	void (*destroy)(void *data);
	void *data;
};

extern const struct wl_seat_listener seat_listener;

void free_hotspots(struct wl_list *list);

#endif
