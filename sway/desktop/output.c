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
		if (strcasecmp(output->name, name) == 0) {
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
	if (rotation == 0.0f) {
		return;
	}

	// Coordinates relative to the center of the subsurface
	double ox = *sx - pw/2 + sw/2,
		oy = *sy - ph/2 + sh/2;
	// Rotated coordinates
	double rx = cos(-rotation)*ox - sin(-rotation)*oy,
		ry = cos(-rotation)*oy + sin(-rotation)*ox;
	*sx = rx + pw/2 - sw/2;
	*sy = ry + ph/2 - sh/2;
}

/**
 * Contains a surface's root geometry information. For instance, when rendering
 * a popup, this will contain the parent view's position and size.
 */
struct root_geometry {
	double x, y;
	int width, height;
	float rotation;
};

static bool get_surface_box(struct root_geometry *geo,
		struct sway_output *output, struct wlr_surface *surface, int sx, int sy,
		struct wlr_box *surface_box) {
	int sw = surface->current->width;
	int sh = surface->current->height;

	double _sx = sx, _sy = sy;
	rotate_child_position(&_sx, &_sy, sw, sh, geo->width, geo->height,
		geo->rotation);

	struct wlr_box box = {
		.x = geo->x + _sx,
		.y = geo->y + _sy,
		.width = sw,
		.height = sh,
	};
	if (surface_box != NULL) {
		memcpy(surface_box, &box, sizeof(struct wlr_box));
	}

	struct wlr_box rotated_box;
	wlr_box_rotated_bounds(&box, geo->rotation, &rotated_box);

	struct wlr_box output_box = {
		.width = output->swayc->width,
		.height = output->swayc->height,
	};

	struct wlr_box intersection;
	return wlr_box_intersection(&output_box, &rotated_box, &intersection);
}

static void output_surface_for_each_surface(struct wlr_surface *surface,
		double ox, double oy, float rotation, struct root_geometry *geo,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	geo->x = ox;
	geo->y = oy;
	geo->width = surface->current->width;
	geo->height = surface->current->height;
	geo->rotation = rotation;

	wlr_surface_for_each_surface(surface, iterator, user_data);
}

static void output_view_for_each_surface(struct sway_view *view,
		struct root_geometry *geo, wlr_surface_iterator_func_t iterator,
		void *user_data) {
	geo->x = view->swayc->x;
	geo->y = view->swayc->y;
	geo->width = view->surface->current->width;
	geo->height = view->surface->current->height;
	geo->rotation = 0; // TODO

	view_for_each_surface(view, iterator, user_data);
}

static void scale_box(struct wlr_box *box, float scale) {
	box->x *= scale;
	box->y *= scale;
	box->width *= scale;
	box->height *= scale;
}

struct render_data {
	struct root_geometry root_geo;
	struct sway_output *output;
	struct timespec *when;
	float alpha;
};

static void render_surface_iterator(struct wlr_surface *surface, int sx, int sy,
		void *_data) {
	struct render_data *data = _data;
	struct wlr_output *wlr_output = data->output->wlr_output;
	struct timespec *when = data->when;
	float rotation = data->root_geo.rotation;
	float alpha = data->alpha;

	if (!wlr_surface_has_buffer(surface)) {
		return;
	}

	struct wlr_box box;
	bool intersects = get_surface_box(&data->root_geo, data->output, surface,
		sx, sy, &box);
	if (!intersects) {
		return;
	}

	struct wlr_renderer *renderer =
		wlr_backend_get_renderer(wlr_output->backend);
	if (!sway_assert(renderer != NULL,
			"expected the output backend to have a renderer")) {
		return;
	}

	scale_box(&box, wlr_output->scale);

	float matrix[9];
	enum wl_output_transform transform =
		wlr_output_transform_invert(surface->current->transform);
	wlr_matrix_project_box(matrix, &box, transform, rotation,
		wlr_output->transform_matrix);

	wlr_render_texture_with_matrix(renderer, surface->texture,
		matrix, alpha);

	// TODO: don't send the frame done event now
	wlr_surface_send_frame_done(surface, when);
}

static void render_surface(struct sway_output *output, struct timespec *when,
		struct wlr_surface *surface, double ox, double oy, float rotation) {
	struct render_data data = {
		.output = output,
		.when = when,
		.alpha = 1.0f,
	};

	output_surface_for_each_surface(surface, ox, oy, rotation, &data.root_geo,
		render_surface_iterator, &data);
}

static void render_view(struct sway_output *output, struct timespec *when,
		struct sway_view *view) {
	struct render_data data = {
		.output = output,
		.when = when,
		.alpha = view->swayc->alpha,
	};

	output_view_for_each_surface(view, &data.root_geo,
		render_surface_iterator, &data);
}

static void render_layer(struct sway_output *output, struct timespec *when,
		struct wl_list *layer_surfaces) {
	struct sway_layer_surface *layer_surface;
	wl_list_for_each(layer_surface, layer_surfaces, link) {
		struct wlr_layer_surface *wlr_layer_surface =
			layer_surface->layer_surface;
		render_surface(output, when, wlr_layer_surface->surface,
			layer_surface->geo.x, layer_surface->geo.y, 0);
	}
}

struct render_view_data {
	struct sway_output *output;
	struct timespec *when;
};

static void render_view_iterator(struct sway_container *con, void *_data) {
	struct render_view_data *data = _data;

	if (!sway_assert(con->type == C_VIEW, "expected a view")) {
		return;
	}

	render_view(data->output, data->when, con->sway_view);
}

static void render_output(struct sway_output *output, struct timespec *when,
		pixman_region32_t *damage) {
	struct wlr_output *wlr_output = output->wlr_output;

	struct wlr_renderer *renderer =
	wlr_backend_get_renderer(wlr_output->backend);
	if (!sway_assert(renderer != NULL,
			"expected the output backend to have a renderer")) {
		return;
	}

	wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);

	if (!pixman_region32_not_empty(damage)) {
		// Output isn't damaged but needs buffer swap
		goto renderer_end;
	}

	float clear_color[] = {0.25f, 0.25f, 0.25f, 1.0f};
	wlr_renderer_clear(renderer, clear_color);

	render_layer(output, when,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
	render_layer(output, when,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

	// Render all views in the current workspace
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus =
		seat_get_focus_inactive(seat, output->swayc);
	if (!focus) {
		// We've never been to this output before
		focus = output->swayc->children->items[0];
	}
	struct sway_container *workspace = focus->type == C_WORKSPACE ?
		focus : container_parent(focus, C_WORKSPACE);
	struct render_view_data data = { .output = output, .when = when };
	container_descendants(workspace, C_VIEW, render_view_iterator, &data);

	// Render unmanaged views on top
	struct wl_list *unmanaged = &root_container.sway_root->xwayland_unmanaged;
	struct sway_xwayland_unmanaged *unmanaged_surface;
	wl_list_for_each(unmanaged_surface, unmanaged, link) {
		struct wlr_xwayland_surface *xsurface =
			unmanaged_surface->wlr_xwayland_surface;
		double ox = unmanaged_surface->lx - output->swayc->x;
		double oy = unmanaged_surface->ly - output->swayc->y;
		render_surface(output, when, xsurface->surface, ox, oy, 0);
	}

	// TODO: consider revising this when fullscreen windows are supported
	render_layer(output, when,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
	render_layer(output, when,
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

void output_damage_surface(struct sway_output *output, double ox, double oy,
		struct wlr_surface *surface, bool whole) {
	// TODO
	output_damage_whole(output);
}

void output_damage_view(struct sway_output *output, struct sway_view *view,
		bool whole) {
	// TODO
	output_damage_whole(output);
}

void output_damage_whole_container(struct sway_output *output,
		struct sway_container *con) {
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

	output->swayc = output_create(output);
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
