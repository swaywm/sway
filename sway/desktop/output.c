#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <strings.h>
#include <wayland-server.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/types/wlr_wl_shell.h>
#include "log.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/tree/view.h"

struct sway_container *output_by_name(const char *name) {
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *output = root_container.children->items[i];
		if (strcasecmp(output->name, name) == 0){
			return output;
		}
	}
	return NULL;
}

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

/**
 * Checks whether a surface at (lx, ly) intersects an output. If `box` is not
 * NULL, it populates it with the surface box in the output, in output-local
 * coordinates.
 */
static bool surface_intersect_output(struct wlr_surface *surface,
		struct wlr_output_layout *output_layout, struct wlr_output *wlr_output,
		double lx, double ly, float rotation, struct wlr_box *box) {
	double ox = lx, oy = ly;
	wlr_output_layout_output_coords(output_layout, wlr_output, &ox, &oy);

	if (box != NULL) {
		box->x = ox * wlr_output->scale;
		box->y = oy * wlr_output->scale;
		box->width = surface->current->width * wlr_output->scale;
		box->height = surface->current->height * wlr_output->scale;
	}

	struct wlr_box layout_box = {
		.x = lx, .y = ly,
		.width = surface->current->width, .height = surface->current->height,
	};
	wlr_box_rotated_bounds(&layout_box, rotation, &layout_box);
	return wlr_output_layout_intersects(output_layout, wlr_output, &layout_box);
}

static void render_surface(struct wlr_surface *surface,
		struct wlr_output *wlr_output, struct timespec *when,
		double lx, double ly, float rotation) {
	struct wlr_renderer *renderer =
		wlr_backend_get_renderer(wlr_output->backend);

	if (!wlr_surface_has_buffer(surface)) {
		return;
	}

	struct wlr_output_layout *layout = root_container.sway_root->output_layout;

	struct wlr_box box;
	bool intersects = surface_intersect_output(surface, layout, wlr_output,
		lx, ly, rotation, &box);
	if (intersects) {
		float matrix[9];
		enum wl_output_transform transform =
			wlr_output_transform_invert(surface->current->transform);
		wlr_matrix_project_box(matrix, &box, transform, rotation,
			wlr_output->transform_matrix);

		// TODO: configurable alpha
		wlr_render_texture_with_matrix(renderer, surface->texture, matrix, 1.0f);

		wlr_surface_send_frame_done(surface, when);
	}

	struct wlr_subsurface *subsurface;
	wl_list_for_each(subsurface, &surface->subsurface_list, parent_link) {
		struct wlr_surface_state *state = subsurface->surface->current;
		double sx = state->subsurface_position.x;
		double sy = state->subsurface_position.y;
		rotate_child_position(&sx, &sy, state->width, state->height,
			surface->current->width, surface->current->height, rotation);

		render_surface(subsurface->surface, wlr_output, when,
			lx + sx, ly + sy, rotation);
	}
}

static void render_xdg_v6_popups(struct wlr_xdg_surface_v6 *surface,
		struct wlr_output *wlr_output, struct timespec *when, double base_x,
		double base_y, float rotation) {
	double width = surface->surface->current->width;
	double height = surface->surface->current->height;

	struct wlr_xdg_popup_v6 *popup_state;
	wl_list_for_each(popup_state, &surface->popups, link) {
		struct wlr_xdg_surface_v6 *popup = popup_state->base;
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

struct render_data {
	struct sway_output *output;
	struct timespec *when;
};

static void render_view(struct sway_container *view, void *data) {
	struct render_data *rdata = data;
	struct sway_output *output = rdata->output;
	struct timespec *when = rdata->when;
	struct wlr_output *wlr_output = output->wlr_output;
	struct sway_view *sway_view = view->sway_view;
	struct wlr_surface *surface = sway_view->surface;

	if (!surface) {
		return;
	}

	switch (sway_view->type) {
	case SWAY_XDG_SHELL_V6_VIEW: {
		int window_offset_x = view->sway_view->wlr_xdg_surface_v6->geometry.x;
		int window_offset_y = view->sway_view->wlr_xdg_surface_v6->geometry.y;
		render_surface(surface, wlr_output, when,
			view->x - window_offset_x, view->y - window_offset_y, 0);
		render_xdg_v6_popups(sway_view->wlr_xdg_surface_v6, wlr_output,
			when, view->x - window_offset_x, view->y - window_offset_y, 0);
		break;
	}
	case SWAY_WL_SHELL_VIEW:
		render_wl_shell_surface(sway_view->wlr_wl_shell_surface, wlr_output,
			when, view->x, view->y, 0, false);
		break;
	case SWAY_XWAYLAND_VIEW:
		render_surface(surface, wlr_output, when, view->x, view->y, 0);
		break;
	default:
		break;
	}
}

static void render_layer(struct sway_output *output,
		const struct wlr_box *output_layout_box,
		struct timespec *when,
		struct wl_list *layer) {
	struct sway_layer_surface *sway_layer;
	wl_list_for_each(sway_layer, layer, link) {
		struct wlr_layer_surface *layer = sway_layer->layer_surface;
		render_surface(layer->surface, output->wlr_output, when,
				sway_layer->geo.x, sway_layer->geo.y, 0);
		wlr_surface_send_frame_done(layer->surface, when);
	}
}

static void render_output(struct sway_output *output, struct timespec *when,
		pixman_region32_t *damage) {
	struct wlr_output *wlr_output = output->wlr_output;
	struct wlr_renderer *renderer =
		wlr_backend_get_renderer(wlr_output->backend);

	wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);

	if (!pixman_region32_not_empty(damage)) {
		// Output isn't damaged but needs buffer swap
		goto renderer_end;
	}

	// TODO: don't damage the whole output here
	int width, height;
	wlr_output_transformed_resolution(wlr_output, &width, &height);
	pixman_region32_union_rect(damage, damage, 0, 0, width, height);

	float clear_color[] = {0.25f, 0.25f, 0.25f, 1.0f};
	wlr_renderer_clear(renderer, clear_color);

	struct wlr_output_layout *layout = root_container.sway_root->output_layout;
	const struct wlr_box *output_box =
			wlr_output_layout_get_box(layout, wlr_output);

	render_layer(output, output_box, when,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
	render_layer(output, output_box, when,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus =
		seat_get_focus_inactive(seat, output->swayc);
	if (!focus) {
		// We've never been to this output before
		focus = output->swayc->children->items[0];
	}
	struct sway_container *workspace = focus->type == C_WORKSPACE ?
			focus : container_parent(focus, C_WORKSPACE);

	struct render_data rdata = {
		.output = output,
		.when = when,
	};
	container_descendants(workspace, C_VIEW, render_view, &rdata);

	// render unmanaged views on top
	struct wl_list *unmanaged = &root_container.sway_root->xwayland_unmanaged;
	struct sway_xwayland_unmanaged *sway_surface;
	wl_list_for_each(sway_surface, unmanaged, link) {
		struct wlr_xwayland_surface *xsurface =
			sway_surface->wlr_xwayland_surface;
		if (xsurface->surface == NULL) {
			continue;
		}

		const struct wlr_box view_box = {
			.x = xsurface->x,
			.y = xsurface->y,
			.width = xsurface->width,
			.height = xsurface->height,
		};
		struct wlr_box intersection;
		if (!wlr_box_intersection(&view_box, output_box, &intersection)) {
			continue;
		}

		render_surface(xsurface->surface, wlr_output, &output->last_frame,
			view_box.x - output_box->x, view_box.y - output_box->y, 0);
	}

	// TODO: Consider revising this when fullscreen windows are supported
	render_layer(output, output_box, when,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
	render_layer(output, output_box, when,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);

renderer_end:
	wlr_renderer_end(renderer);
	if (!wlr_output_damage_swap_buffers(output->damage, when, damage)) {
		return;
	}
	output->last_frame = *when;
}

static void damage_handle_frame(struct wl_listener *listener, void *data) {
	struct sway_output *output =
		wl_container_of(listener, output, damage_frame);

	if (!output->wlr_output->enabled) {
		return;
	}

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	bool needs_swap;
	pixman_region32_t damage;
	pixman_region32_init(&damage);
	if (!wlr_output_damage_make_current(output->damage, &needs_swap, &damage)) {
		return;
	}

	if (needs_swap) {
		render_output(output, &now, &damage);
	}

	pixman_region32_fini(&damage);

	// TODO: send frame done events here instead of inside render_surface
}

void output_damage_whole(struct sway_output *output) {
	wlr_output_damage_add_whole(output->damage);
}

void output_damage_whole_view(struct sway_output *output,
		struct sway_view *view) {
	// TODO
	output_damage_whole(output);
}

static void damage_handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_output *output =
		wl_container_of(listener, output, damage_destroy);
	container_destroy(output->swayc);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, destroy);
	container_destroy(output->swayc);
}

static void handle_mode(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, mode);
	arrange_layers(output);
	arrange_windows(output->swayc, -1, -1);
}

static void handle_transform(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, transform);
	arrange_layers(output);
	arrange_windows(output->swayc, -1, -1);
}

static void handle_scale(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, scale);
	arrange_layers(output);
	arrange_windows(output->swayc, -1, -1);
}

void handle_new_output(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;
	wlr_log(L_DEBUG, "New output %p: %s", wlr_output, wlr_output->name);

	struct sway_output *output = calloc(1, sizeof(struct sway_output));
	if (!output) {
		return;
	}
	output->wlr_output = wlr_output;
	wlr_output->data = output;
	output->server = server;

	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode =
			wl_container_of(wlr_output->modes.prev, mode, link);
		wlr_output_set_mode(wlr_output, mode);
	}

	output->damage = wlr_output_damage_create(wlr_output);

	output->swayc = container_output_create(output);
	if (!output->swayc) {
		free(output);
		return;
	}

	size_t len = sizeof(output->layers) / sizeof(output->layers[0]);
	for (size_t i = 0; i < len; ++i) {
		wl_list_init(&output->layers[i]);
	}

	input_manager_configure_xcursor(input_manager);

	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	output->destroy.notify = handle_destroy;
	wl_signal_add(&wlr_output->events.mode, &output->mode);
	output->mode.notify = handle_mode;
	wl_signal_add(&wlr_output->events.transform, &output->transform);
	output->transform.notify = handle_transform;
	wl_signal_add(&wlr_output->events.scale, &output->scale);
	output->scale.notify = handle_scale;

	wl_signal_add(&output->damage->events.frame, &output->damage_frame);
	output->damage_frame.notify = damage_handle_frame;
	wl_signal_add(&output->damage->events.destroy, &output->damage_destroy);
	output->damage_destroy.notify = damage_handle_destroy;

	arrange_layers(output);
	arrange_windows(&root_container, -1, -1);
}
