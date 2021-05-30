#ifndef _SWAYBAR_TRAY_DBUSMENU_H
#define _SWAYBAR_TRAY_DBUSMENU_H

#include "swaybar/bar.h"
#include "swaybar/tray/item.h"

void swaybar_dbusmenu_open(struct swaybar_sni *sni,
		struct swaybar_output *output, struct swaybar_seat *seat, uint32_t serial,
		int x, int y);

bool dbusmenu_pointer_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time_, uint32_t button, uint32_t state);

bool dbusmenu_pointer_motion(struct swaybar_seat *seat, struct wl_pointer *wl_pointer,
		uint32_t time_, wl_fixed_t surface_x, wl_fixed_t surface_y);

bool dbusmenu_pointer_enter(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
		struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y);

bool dbusmenu_pointer_leave(void *data, struct wl_pointer *wl_pointer, uint32_t serial,
		struct wl_surface *surface);

bool dbusmenu_pointer_frame(struct swaybar_seat *data, struct wl_pointer *wl_pointer);

bool dbusmenu_pointer_axis(struct swaybar_seat *data, struct wl_pointer *wl_pointer);

#endif
