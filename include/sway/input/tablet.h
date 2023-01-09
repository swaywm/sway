#ifndef _SWAY_INPUT_TABLET_H
#define _SWAY_INPUT_TABLET_H
#include <wlr/types/wlr_layer_shell_v1.h>

struct sway_seat;
struct wlr_tablet_tool;

struct sway_tablet {
	struct wl_list link;
	struct sway_seat_device *seat_device;
	struct wlr_tablet_v2_tablet *tablet_v2;
};

enum sway_tablet_tool_mode {
	SWAY_TABLET_TOOL_MODE_ABSOLUTE,
	SWAY_TABLET_TOOL_MODE_RELATIVE,
};

struct sway_tablet_tool {
	struct sway_seat *seat;
	struct sway_tablet *tablet;
	struct wlr_tablet_v2_tablet_tool *tablet_v2_tool;

	enum sway_tablet_tool_mode mode;
	double tilt_x, tilt_y;

	struct wl_listener set_cursor;
	struct wl_listener tool_destroy;
};

struct sway_tablet_pad {
	struct wl_list link;
	struct sway_seat_device *seat_device;
	struct sway_tablet *tablet;
	struct wlr_tablet_pad *wlr;
	struct wlr_tablet_v2_tablet_pad *tablet_v2_pad;

	struct wl_listener attach;
	struct wl_listener button;
	struct wl_listener ring;
	struct wl_listener strip;

	struct wlr_surface *current_surface;
	struct wl_listener surface_destroy;

	struct wl_listener tablet_destroy;
};

struct sway_tablet *sway_tablet_create(struct sway_seat *seat,
		struct sway_seat_device *device);

void sway_configure_tablet(struct sway_tablet *tablet);

void sway_tablet_destroy(struct sway_tablet *tablet);

void sway_tablet_tool_configure(struct sway_tablet *tablet,
		struct wlr_tablet_tool *wlr_tool);

struct sway_tablet_pad *sway_tablet_pad_create(struct sway_seat *seat,
		struct sway_seat_device *device);

void sway_configure_tablet_pad(struct sway_tablet_pad *tablet_pad);

void sway_tablet_pad_destroy(struct sway_tablet_pad *tablet_pad);

void sway_tablet_pad_set_focus(struct sway_tablet_pad *tablet_pad,
		struct wlr_surface *surface);

#endif
