#ifndef _SWAYBAR_INPUT_H
#define _SWAYBAR_INPUT_H

#include <wayland-client.h>
#include "list.h"

#define SWAY_SCROLL_UP KEY_MAX + 1
#define SWAY_SCROLL_DOWN KEY_MAX + 2
#define SWAY_SCROLL_LEFT KEY_MAX + 3
#define SWAY_SCROLL_RIGHT KEY_MAX + 4

struct swaybar;
struct swaybar_output;

struct swaybar_pointer {
	struct wl_pointer *pointer;
	struct wl_cursor_theme *cursor_theme;
	struct wl_cursor_image *cursor_image;
	struct wl_surface *cursor_surface;
	struct swaybar_output *current;
	int x, y;
	uint32_t serial;
};

enum hotspot_event_handling {
	HOTSPOT_IGNORE,
	HOTSPOT_PROCESS,
};

struct swaybar_hotspot {
	struct wl_list link; // swaybar_output::hotspots
	int x, y, width, height;
	enum hotspot_event_handling (*callback)(struct swaybar_output *output,
		struct swaybar_hotspot *hotspot, int x, int y, uint32_t button,
		void *data);
	void (*destroy)(void *data);
	void *data;
};

extern const struct wl_seat_listener seat_listener;

void update_cursor(struct swaybar *bar);

uint32_t event_to_x11_button(uint32_t event);

void free_hotspots(struct wl_list *list);

#endif
