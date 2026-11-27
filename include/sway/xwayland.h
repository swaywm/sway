#ifndef SWAY_XWAYLAND_H
#define SWAY_XWAYLAND_H

#include <wlr/xwayland.h>
#include <xcb/xproto.h>

enum atom_name {
	NET_WM_WINDOW_TYPE_NORMAL,
	NET_WM_WINDOW_TYPE_DIALOG,
	NET_WM_WINDOW_TYPE_UTILITY,
	NET_WM_WINDOW_TYPE_TOOLBAR,
	NET_WM_WINDOW_TYPE_SPLASH,
	NET_WM_WINDOW_TYPE_MENU,
	NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
	NET_WM_WINDOW_TYPE_POPUP_MENU,
	NET_WM_WINDOW_TYPE_TOOLTIP,
	NET_WM_WINDOW_TYPE_NOTIFICATION,
	NET_WM_STATE_MODAL,
	ATOM_LAST,
};

struct sway_xwayland {
	struct wlr_xwayland *wlr_xwayland;
	struct wlr_xcursor_manager *xcursor_manager;

	struct wl_list docks; // sway_xwayland_dock::link, in mapping order

	xcb_atom_t atoms[ATOM_LAST];
};

struct sway_output;
struct wlr_box;

/**
 * Places the X11 docks of an output against the top and bottom edges of its
 * usable area, in the order they were mapped, and removes their height from
 * the usable area.
 */
void arrange_xwayland_docks(struct sway_output *output,
	const struct wlr_box *full_area, struct wlr_box *usable_area);

/**
 * Moves the X11 docks of an output that is being disabled to the first
 * remaining output.
 */
void evacuate_xwayland_docks(struct sway_output *output);

void handle_xwayland_ready(struct wl_listener *listener, void *data);

#endif
