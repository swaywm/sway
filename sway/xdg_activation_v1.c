#include <wlr/types/wlr_xdg_activation_v1.h>
#include "sway/desktop/launcher.h"
#include "sway/tree/view.h"

void xdg_activation_v1_handle_request_activate(struct wl_listener *listener,
		void *data) {
	const struct wlr_xdg_activation_v1_request_activate_event *event = data;

	if (!wlr_surface_is_xdg_surface(event->surface)) {
		return;
	}

	struct wlr_xdg_surface *xdg_surface =
		wlr_xdg_surface_from_wlr_surface(event->surface);
	if (xdg_surface == NULL) {
		return;
	}
	struct sway_view *view = xdg_surface->data;
	if (view == NULL) {
		return;
	}

	if (!xdg_surface->mapped) {
		// This is a startup notification. If we are tracking it, the data
		// field is a launcher_ctx.
		struct launcher_ctx *ctx = event->token->data;
		if (!ctx || ctx->activated) {
			// This ctx has already been activated and cannot be used again
			// for a startup notification. It will be destroyed
			return;
		} else {
			ctx->activated = true;
			view_assign_ctx(view, ctx);
		}
		return;
	}

	view_request_activate(view);
}
