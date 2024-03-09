#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
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

	struct launcher_ctx *ctx = event->token->data;
	if (ctx == NULL) {
		return;
	}

	if (!xdg_surface->surface->mapped) {
		// This is a startup notification. If we are tracking it, the data
		// field is a launcher_ctx.
		if (ctx->activated) {
			// This ctx has already been activated and cannot be used again
			// for a startup notification. It will be destroyed
			return;
		} else {
			ctx->activated = true;
			view_assign_ctx(view, ctx);
		}
		return;
	}

	// This is an activation request. If this context is internal we have ctx->seat.
	struct sway_seat *seat = ctx->seat;
	if (!seat) {
		// Otherwise, use the seat indicated by the launcher client in set_serial
		seat = ctx->token->seat ? ctx->token->seat->data : NULL;
	}

	if (seat && ctx->had_focused_surface) {
		view_request_activate(view, seat);
	} else {
		// The token is valid, but cannot be used to activate a window
		view_request_urgent(view);
	}
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
