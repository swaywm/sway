#include <wlr/types/wlr_xdg_activation_v1.h>
#include "sway/tree/view.h"

void xdg_activation_v1_handle_request_activate(struct wl_listener *listener,
		void *data) {
	const struct wlr_xdg_activation_v1_request_activate_event *event = data;

	if (!wlr_surface_is_xdg_surface(event->surface)) {
		return;
	}

	struct wlr_xdg_surface *xdg_surface =
		wlr_xdg_surface_from_wlr_surface(event->surface);
	struct sway_view *view = xdg_surface->data;
	if (!xdg_surface->mapped || view == NULL) {
		return;
	}

	view_request_activate(view);
}
