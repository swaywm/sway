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
	NET_SUPPORTED,
	NET_MOVERESIZE_WINDOW,
	NET_FRAME_EXTENTS,
	NET_REQUEST_FRAME_EXTENTS,
	NET_WM_STATE,
	NET_WM_STATE_STICKY,
	WM_CLIENT_LEADER,
	ATOM_LAST,
};

struct sway_xwayland {
	struct wlr_xwayland *wlr_xwayland;
	struct wlr_xcursor_manager *xcursor_manager;

	xcb_atom_t atoms[ATOM_LAST];
};

void handle_xwayland_ready(struct wl_listener *listener, void *data);

/**
 * Handles the X11 events of the XWM's connection that need sway's view of the
 * tree, such as client messages to the root window. Returns true if the XWM
 * must not handle the event as well.
 */
bool handle_xwayland_event(struct wlr_xwayland *wlr_xwayland,
	xcb_generic_event_t *event);

#endif
