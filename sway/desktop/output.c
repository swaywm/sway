#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <wayland-server.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/types/wlr_wl_shell.h>
#include <wlr/render.h>
#include <wlr/render/matrix.h>
#include "log.h"
#include "sway/container.h"
#include "sway/layout.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/view.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"

/**
 * Rotate a child's position relative to a parent. The parent size is (pw, ph),
 * the child position is (*sx, *sy) and its size is (sw, sh).
 */
static void rotate_child_position(double *sx, double *sy, double sw, double sh,
		double pw, double ph, float rotation) {
	if (rotation != 0.0) {
		// Coordinates relative to the center of the subsurface
		double ox = *sx - pw/2 + sw/2,
			oy = *sy - ph/2 + sh/2;
		// Rotated coordinates
		double rx = cos(-rotation)*ox - sin(-rotation)*oy,
			ry = cos(-rotation)*oy + sin(-rotation)*ox;
		*sx = rx + pw/2 - sw/2;
		*sy = ry + ph/2 - sh/2;
	}
}

static void render_surface(struct wlr_surface *surface,
		struct wlr_output *wlr_output, struct timespec *when,
		double lx, double ly, float rotation) {
	if (!wlr_surface_has_buffer(surface)) {
		return;
	}
	struct wlr_output_layout *layout = root_container.sway_root->output_layout;
	int width = surface->current->width;
	int height = surface->current->height;
	int render_width = width * wlr_output->scale;
	int render_height = height * wlr_output->scale;
	double ox = lx, oy = ly;
	wlr_output_layout_output_coords(layout, wlr_output, &ox, &oy);
	ox *= wlr_output->scale;
	oy *= wlr_output->scale;

	struct wlr_box render_box = {
		.x = lx, .y = ly,
		.width = render_width, .height = render_height,
	};
	if (wlr_output_layout_intersects(layout, wlr_output, &render_box)) {
		float matrix[16];

		float translate_center[16];
		wlr_matrix_translate(&translate_center,
			(int)ox + render_width / 2, (int)oy + render_height / 2, 0);

		float rotate[16];
		wlr_matrix_rotate(&rotate, rotation);

		float translate_origin[16];
		wlr_matrix_translate(&translate_origin, -render_width / 2,
			-render_height / 2, 0);

		float scale[16];
		wlr_matrix_scale(&scale, render_width, render_height, 1);

		float transform[16];
		wlr_matrix_mul(&translate_center, &rotate, &transform);
		wlr_matrix_mul(&transform, &translate_origin, &transform);
		wlr_matrix_mul(&transform, &scale, &transform);

		if (surface->current->transform != WL_OUTPUT_TRANSFORM_NORMAL) {
			float surface_translate_center[16];
			wlr_matrix_translate(&surface_translate_center, 0.5, 0.5, 0);

			float surface_transform[16];
			wlr_matrix_transform(surface_transform,
				wlr_output_transform_invert(surface->current->transform));

			float surface_translate_origin[16];
			wlr_matrix_translate(&surface_translate_origin, -0.5, -0.5, 0);

			wlr_matrix_mul(&transform, &surface_translate_center,
				&transform);
			wlr_matrix_mul(&transform, &surface_transform, &transform);
			wlr_matrix_mul(&transform, &surface_translate_origin,
				&transform);
		}

		wlr_matrix_mul(&wlr_output->transform_matrix, &transform, &matrix);

		wlr_render_with_matrix(server.renderer, surface->texture,
			&matrix);

		wlr_surface_send_frame_done(surface, when);
	}

	struct wlr_subsurface *subsurface;
	wl_list_for_each(subsurface, &surface->subsurface_list, parent_link) {
		struct wlr_surface_state *state = subsurface->surface->current;
		double sx = state->subsurface_position.x;
		double sy = state->subsurface_position.y;
		double sw = state->buffer_width / state->scale;
		double sh = state->buffer_height / state->scale;
		rotate_child_position(&sx, &sy, sw, sh, width, height, rotation);

		render_surface(subsurface->surface, wlr_output, when,
			lx + sx,
			ly + sy,
			rotation);
	}
}

static void render_xdg_v6_popups(struct wlr_xdg_surface_v6 *surface,
		struct wlr_output *wlr_output, struct timespec *when, double base_x,
		double base_y, float rotation) {
	double width = surface->surface->current->width;
	double height = surface->surface->current->height;

	struct wlr_xdg_surface_v6 *popup;
	wl_list_for_each(popup, &surface->popups, popup_link) {
		if (!popup->configured) {
			continue;
		}

		double popup_width = popup->surface->current->width;
		double popup_height = popup->surface->current->height;

		double popup_sx, popup_sy;
		wlr_xdg_surface_v6_popup_get_position(popup, &popup_sx, &popup_sy);
		rotate_child_position(&popup_sx, &popup_sy, popup_width, popup_height,
			width, height, rotation);

		render_surface(popup->surface, wlr_output, when,
			base_x + popup_sx, base_y + popup_sy, rotation);
		render_xdg_v6_popups(popup, wlr_output, when,
			base_x + popup_sx, base_y + popup_sy, rotation);
	}
}

static void render_wl_shell_surface(struct wlr_wl_shell_surface *surface,
		struct wlr_output *wlr_output, struct timespec *when,
		double lx, double ly, float rotation,
		bool is_child) {
	if (is_child || surface->state != WLR_WL_SHELL_SURFACE_STATE_POPUP) {
		render_surface(surface->surface, wlr_output, when,
			lx, ly, rotation);

		double width = surface->surface->current->width;
		double height = surface->surface->current->height;

		struct wlr_wl_shell_surface *popup;
		wl_list_for_each(popup, &surface->popups, popup_link) {
			double popup_width = popup->surface->current->width;
			double popup_height = popup->surface->current->height;

			double popup_x = popup->transient_state->x;
			double popup_y = popup->transient_state->y;
			rotate_child_position(&popup_x, &popup_y, popup_width, popup_height,
				width, height, rotation);

			render_wl_shell_surface(popup, wlr_output, when,
				lx + popup_x, ly + popup_y, rotation, true);
		}
	}
}


static void output_frame_view(swayc_t *view, void *data) {
	struct sway_output *output = data;
	struct wlr_output *wlr_output = output->wlr_output;
	struct sway_view *sway_view = view->sway_view;
	struct wlr_surface *surface = sway_view->surface;

	switch (sway_view->type) {
	case SWAY_XDG_SHELL_V6_VIEW: {
		int window_offset_x = view->sway_view->wlr_xdg_surface_v6->geometry->x;
		int window_offset_y = view->sway_view->wlr_xdg_surface_v6->geometry->y;
		render_surface(surface, wlr_output, &output->last_frame,
			view->x - window_offset_x,
			view->y - window_offset_y,
			0);
		render_xdg_v6_popups(sway_view->wlr_xdg_surface_v6, wlr_output,
			&output->last_frame,
			view->x - window_offset_x, view->y - window_offset_y,
			0);
		break;
	}
	case SWAY_WL_SHELL_VIEW:
		render_wl_shell_surface(sway_view->wlr_wl_shell_surface, wlr_output,
			&output->last_frame, view->x, view->y, 0, false);
		break;
	case SWAY_XWAYLAND_VIEW:
		render_surface(surface, wlr_output, &output->last_frame, view->x,
			view->y, 0);
		break;
	default:
		break;
	}
}

static void output_frame_notify(struct wl_listener *listener, void *data) {
	struct sway_output *soutput = wl_container_of(listener, soutput, frame);
	struct wlr_output *wlr_output = data;
	struct sway_server *server = soutput->server;

	wlr_output_make_current(wlr_output);
	wlr_renderer_begin(server->renderer, wlr_output);

	swayc_descendants_of_type(
			&root_container, C_VIEW, output_frame_view, soutput);

	// render unmanaged views on top
	struct sway_view *view;
	wl_list_for_each(view, &root_container.sway_root->unmanaged_views,
			unmanaged_view_link) {
		if (view->type == SWAY_XWAYLAND_VIEW) {
			// the only kind of unamanged view right now is xwayland override redirect
			int view_x = view->wlr_xwayland_surface->x;
			int view_y = view->wlr_xwayland_surface->y;
			render_surface(view->surface, wlr_output, &soutput->last_frame,
					view_x, view_y, 0);
		}
	}

	wlr_renderer_end(server->renderer);
	wlr_output_swap_buffers(wlr_output);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	soutput->last_frame = now;
}

void output_add_notify(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(listener, server, output_add);
	struct wlr_output *wlr_output = data;
	wlr_log(L_DEBUG, "New output %p: %s", wlr_output, wlr_output->name);

	struct sway_output *output = calloc(1, sizeof(struct sway_output));
	if (!output) {
		return;
	}
	output->wlr_output = wlr_output;
	output->server = server;

	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode =
			wl_container_of(wlr_output->modes.prev, mode, link);
		wlr_output_set_mode(wlr_output, mode);
	}

	output->swayc = new_output(output);
	if (!output->swayc) {
		free(output);
		return;
	}

	sway_input_manager_configure_xcursor(input_manager);

	output->frame.notify = output_frame_notify;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
}

void output_remove_notify(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(listener, server, output_remove);
	struct wlr_output *wlr_output = data;
	wlr_log(L_DEBUG, "Output %p %s removed", wlr_output, wlr_output->name);

	swayc_t *output_container = NULL;
	for (int i = 0 ; i < root_container.children->length; ++i) {
		swayc_t *child = root_container.children->items[i];
		if (child->type == C_OUTPUT &&
				child->sway_output->wlr_output == wlr_output) {
			output_container = child;
			break;
		}
	}
	if (!output_container) {
		return;
	}

	destroy_output(output_container);
}
