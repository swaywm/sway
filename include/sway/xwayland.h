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
	NET_NUMBER_OF_DESKTOPS,
	NET_DESKTOP_NAMES,
	NET_CURRENT_DESKTOP,
	NET_DESKTOP_VIEWPORT,
	NET_WM_DESKTOP,
	UTF8_STRING,
	ATOM_LAST,
};

struct sway_xwayland {
	struct wlr_xwayland *wlr_xwayland;
	struct wlr_xcursor_manager *xcursor_manager;

	xcb_atom_t atoms[ATOM_LAST];

	// EWMH desktops, as last published on the root window of the X server.
	// root is XCB_WINDOW_NONE until the X window manager is ready.
	xcb_window_t root;
	struct wl_event_source *desktops_idle;
	struct {
		bool published;
		uint32_t count;
		uint32_t current;
		char *names;
		size_t names_len;
		uint32_t *viewport;
	} desktops;
};

void handle_xwayland_ready(struct wl_listener *listener, void *data);

bool handle_xwayland_event(struct wlr_xwayland *wlr_xwayland,
	xcb_generic_event_t *event);

/**
 * Schedule an update of the EWMH desktop properties (_NET_NUMBER_OF_DESKTOPS,
 * _NET_DESKTOP_NAMES, _NET_CURRENT_DESKTOP, _NET_DESKTOP_VIEWPORT and each
 * managed window's _NET_WM_DESKTOP) seen by X11 clients.
 */
void xwayland_schedule_desktops_update(void);

void xwayland_desktops_finish(struct sway_xwayland *xwayland);

#endif
