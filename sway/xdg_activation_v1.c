#include <wlr/types/wlr_xdg_activation_v1.h>
#include "sway/desktop/launcher.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

void xdg_activation_v1_handle_request_activate(struct wl_listener *listener,
		void *data) {
	const struct wlr_xdg_activation_v1_request_activate_event *event = data;

	struct wlr_xdg_surface *xdg_surface =
		wlr_xdg_surface_try_from_wlr_surface(event->surface);
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

	struct wlr_seat *wlr_seat = event->token->seat;
	struct sway_seat *seat = wlr_seat ? wlr_seat->data : NULL;
	view_request_activate(view, seat);
}

void xdg_activation_v1_handle_new_token(struct wl_listener *listener, void *data) {
	struct wlr_xdg_activation_token_v1 *token = data;
	struct sway_seat *seat = token->seat ? token->seat->data :
		input_manager_current_seat();

	struct sway_workspace *ws = seat_get_focused_workspace(seat);
	if (ws) {
		launcher_ctx_create(token, &ws->node);
		return;
	}
}
