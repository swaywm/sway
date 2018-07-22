#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <wayland-server.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/util/region.h>
#include "log.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

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

bool output_get_surface_box(struct root_geometry *geo,
		struct sway_output *output, struct wlr_surface *surface, int sx, int sy,
		struct wlr_box *surface_box) {
	if (!wlr_surface_has_buffer(surface)) {
		return false;
	}

	int sw = surface->current.width;
	int sh = surface->current.height;

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
		.width = output->swayc->current.swayc_width,
		.height = output->swayc->current.swayc_height,
	};

	struct wlr_box intersection;
	return wlr_box_intersection(&output_box, &rotated_box, &intersection);
}

void output_surface_for_each_surface(struct wlr_surface *surface,
		double ox, double oy, struct root_geometry *geo,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	geo->x = ox;
	geo->y = oy;
	geo->width = surface->current.width;
	geo->height = surface->current.height;
	geo->rotation = 0;

	wlr_surface_for_each_surface(surface, iterator, user_data);
}

void output_view_for_each_surface(struct sway_view *view,
		struct sway_output *output, struct root_geometry *geo,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	geo->x = view->swayc->current.view_x - output->swayc->current.swayc_x;
	geo->y = view->swayc->current.view_y - output->swayc->current.swayc_y;
	geo->width = view->swayc->current.view_width;
	geo->height = view->swayc->current.view_height;
	geo->rotation = 0; // TODO

	view_for_each_surface(view, iterator, user_data);
}

void output_layer_for_each_surface(struct wl_list *layer_surfaces,
		struct root_geometry *geo, wlr_surface_iterator_func_t iterator,
		void *user_data) {
	struct sway_layer_surface *layer_surface;
	wl_list_for_each(layer_surface, layer_surfaces, link) {
		struct wlr_layer_surface *wlr_layer_surface =
			layer_surface->layer_surface;
		output_surface_for_each_surface(wlr_layer_surface->surface,
			layer_surface->geo.x, layer_surface->geo.y, geo, iterator,
			user_data);
	}
}

void output_unmanaged_for_each_surface(struct wl_list *unmanaged,
		struct sway_output *output, struct root_geometry *geo,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	struct sway_xwayland_unmanaged *unmanaged_surface;
	wl_list_for_each(unmanaged_surface, unmanaged, link) {
		struct wlr_xwayland_surface *xsurface =
			unmanaged_surface->wlr_xwayland_surface;
		double ox = unmanaged_surface->lx - output->swayc->current.swayc_x;
		double oy = unmanaged_surface->ly - output->swayc->current.swayc_y;

		output_surface_for_each_surface(xsurface->surface, ox, oy, geo,
			iterator, user_data);
	}
}

void output_drag_icons_for_each_surface(struct wl_list *drag_icons,
		struct sway_output *output, struct root_geometry *geo,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	struct sway_drag_icon *drag_icon;
	wl_list_for_each(drag_icon, drag_icons, link) {
		double ox = drag_icon->x - output->swayc->x;
		double oy = drag_icon->y - output->swayc->y;

		if (drag_icon->wlr_drag_icon->mapped) {
			output_surface_for_each_surface(drag_icon->wlr_drag_icon->surface,
				ox, oy, geo, iterator, user_data);
		}
	}
}

static void scale_box(struct wlr_box *box, float scale) {
	box->x *= scale;
	box->y *= scale;
	box->width *= scale;
	box->height *= scale;
}

struct sway_container *output_get_active_workspace(struct sway_output *output) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus =
		seat_get_focus_inactive(seat, output->swayc);
	if (!focus) {
		// We've never been to this output before
		focus = output->swayc->current.children->items[0];
	}
	struct sway_container *workspace = focus;
	if (workspace->type != C_WORKSPACE) {
		workspace = container_parent(workspace, C_WORKSPACE);
	}
	return workspace;
}

bool output_has_opaque_lockscreen(struct sway_output *output,
		struct sway_seat *seat) {
	if (!seat->exclusive_client) {
		return false;
	}

	struct wlr_layer_surface *wlr_layer_surface;
	wl_list_for_each(wlr_layer_surface, &server.layer_shell->surfaces, link) {
		if (wlr_layer_surface->output != output->wlr_output) {
			continue;
		}
		struct wlr_surface *wlr_surface = wlr_layer_surface->surface;
		if (wlr_surface->resource->client != seat->exclusive_client) {
			continue;
		}
		struct sway_layer_surface *sway_layer_surface =
			layer_from_wlr_layer_surface(wlr_layer_surface);
		pixman_box32_t output_box = {
			.x2 = output->swayc->current.swayc_width,
			.y2 = output->swayc->current.swayc_height,
		};
		pixman_region32_t surface_opaque_box;
		pixman_region32_init(&surface_opaque_box);
		pixman_region32_copy(&surface_opaque_box, &wlr_surface->opaque_region);
		pixman_region32_translate(&surface_opaque_box,
			sway_layer_surface->geo.x, sway_layer_surface->geo.y);
		bool contains = pixman_region32_contains_rectangle(&surface_opaque_box,
			&output_box);
		pixman_region32_fini(&surface_opaque_box);
		if (contains) {
			return true;
		}
	}
	return false;
}

struct send_frame_done_data {
	struct root_geometry root_geo;
	struct sway_output *output;
	struct timespec *when;
	struct wl_client *exclusive_client;
};

static void send_frame_done_iterator(struct wlr_surface *surface,
		int sx, int sy, void *_data) {
	struct send_frame_done_data *data = _data;
	if (data->exclusive_client &&
			data->exclusive_client != surface->resource->client) {
		return;
	}

	bool intersects = output_get_surface_box(&data->root_geo, data->output, surface,
		sx, sy, NULL);
	if (intersects) {
		wlr_surface_send_frame_done(surface, data->when);
	}
}

static void send_frame_done_layer(struct send_frame_done_data *data,
		struct wl_list *layer_surfaces) {
	output_layer_for_each_surface(layer_surfaces, &data->root_geo,
		send_frame_done_iterator, data);
}

static void send_frame_done_unmanaged(struct send_frame_done_data *data,
		struct wl_list *unmanaged) {
	output_unmanaged_for_each_surface(unmanaged, data->output, &data->root_geo,
		send_frame_done_iterator, data);
}

static void send_frame_done_drag_icons(struct send_frame_done_data *data,
		struct wl_list *drag_icons) {
	output_drag_icons_for_each_surface(drag_icons, data->output, &data->root_geo,
		send_frame_done_iterator, data);
}

static void send_frame_done_container_iterator(struct sway_container *con,
		void *_data) {
	struct send_frame_done_data *data = _data;
	if (!sway_assert(con->type == C_VIEW, "expected a view")) {
		return;
	}

	if (!view_is_visible(con->sway_view)) {
		return;
	}

	output_view_for_each_surface(con->sway_view, data->output, &data->root_geo,
		send_frame_done_iterator, data);
}

static void send_frame_done_container(struct send_frame_done_data *data,
		struct sway_container *con) {
	container_descendants(con, C_VIEW,
		send_frame_done_container_iterator, data);
}

static void send_frame_done(struct sway_output *output, struct timespec *when) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct send_frame_done_data data = {
		.output = output,
		.when = when,
		.exclusive_client = output_has_opaque_lockscreen(output, seat) ?
			seat->exclusive_client : NULL,
	};

	struct sway_container *workspace = output_get_active_workspace(output);
	if (workspace->current.ws_fullscreen) {
		send_frame_done_container_iterator(
			workspace->current.ws_fullscreen->swayc, &data);

		if (workspace->current.ws_fullscreen->type == SWAY_VIEW_XWAYLAND) {
			send_frame_done_unmanaged(&data,
				&root_container.sway_root->xwayland_unmanaged);
		}
	} else {
		send_frame_done_layer(&data,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
		send_frame_done_layer(&data,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

		send_frame_done_container(&data, workspace);
		send_frame_done_container(&data, workspace->sway_workspace->floating);

		send_frame_done_unmanaged(&data,
			&root_container.sway_root->xwayland_unmanaged);
		send_frame_done_layer(&data,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
	}

	send_frame_done_layer(&data,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);
	send_frame_done_drag_icons(&data, &root_container.sway_root->drag_icons);
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
		output_render(output, &now, &damage);
	}

	pixman_region32_fini(&damage);

	// Send frame done to all visible surfaces
	send_frame_done(output, &now);
}

void output_damage_whole(struct sway_output *output) {
	wlr_output_damage_add_whole(output->damage);
}

struct damage_data {
	struct root_geometry root_geo;
	struct sway_output *output;
	bool whole;
};

static void damage_surface_iterator(struct wlr_surface *surface, int sx, int sy,
		void *_data) {
	struct damage_data *data = _data;
	struct sway_output *output = data->output;
	float rotation = data->root_geo.rotation;
	bool whole = data->whole;

	struct wlr_box box;
	bool intersects = output_get_surface_box(&data->root_geo, data->output, surface,
		sx, sy, &box);
	if (!intersects) {
		return;
	}

	scale_box(&box, output->wlr_output->scale);

	int center_x = box.x + box.width/2;
	int center_y = box.y + box.height/2;

	if (pixman_region32_not_empty(&surface->buffer_damage)) {
		enum wl_output_transform transform =
			wlr_output_transform_invert(surface->current.transform);

		pixman_region32_t damage;
		pixman_region32_init(&damage);
		pixman_region32_copy(&damage, &surface->buffer_damage);
		wlr_region_transform(&damage, &damage, transform,
			surface->current.buffer_width, surface->current.buffer_height);
		wlr_region_scale(&damage, &damage,
			output->wlr_output->scale / (float)surface->current.scale);
		if (ceil(output->wlr_output->scale) > surface->current.scale) {
			// When scaling up a surface, it'll become blurry so we need to
			// expand the damage region
			wlr_region_expand(&damage, &damage,
				ceil(output->wlr_output->scale) - surface->current.scale);
		}
		pixman_region32_translate(&damage, box.x, box.y);
		wlr_region_rotated_bounds(&damage, &damage, rotation,
			center_x, center_y);
		wlr_output_damage_add(output->damage, &damage);
		pixman_region32_fini(&damage);
	}

	if (whole) {
		wlr_box_rotated_bounds(&box, rotation, &box);
		wlr_output_damage_add_box(output->damage, &box);
	}

	wlr_output_schedule_frame(output->wlr_output);
}

void output_damage_surface(struct sway_output *output, double ox, double oy,
		struct wlr_surface *surface, bool whole) {
	struct damage_data data = {
		.output = output,
		.whole = whole,
	};

	output_surface_for_each_surface(surface, ox, oy, &data.root_geo,
		damage_surface_iterator, &data);
}

static void output_damage_view(struct sway_output *output,
		struct sway_view *view, bool whole) {
	if (!sway_assert(view->swayc != NULL, "expected a view in the tree")) {
		return;
	}

	if (!view_is_visible(view)) {
		return;
	}

	struct damage_data data = {
		.output = output,
		.whole = whole,
	};

	output_view_for_each_surface(view, output, &data.root_geo,
		damage_surface_iterator, &data);
}

void output_damage_from_view(struct sway_output *output,
		struct sway_view *view) {
	output_damage_view(output, view, false);
}

// Expecting an unscaled box in layout coordinates
void output_damage_box(struct sway_output *output, struct wlr_box *_box) {
	struct wlr_box box;
	memcpy(&box, _box, sizeof(struct wlr_box));
	box.x -= output->swayc->current.swayc_x;
	box.y -= output->swayc->current.swayc_y;
	scale_box(&box, output->wlr_output->scale);
	wlr_output_damage_add_box(output->damage, &box);
}

static void output_damage_whole_container_iterator(struct sway_container *con,
		void *data) {
	struct sway_output *output = data;

	if (!sway_assert(con->type == C_VIEW, "expected a view")) {
		return;
	}

	output_damage_view(output, con->sway_view, true);
}

void output_damage_whole_container(struct sway_output *output,
		struct sway_container *con) {
	// Pad the box by 1px, because the width is a double and might be a fraction
	struct wlr_box box = {
		.x = con->current.swayc_x - output->wlr_output->lx - 1,
		.y = con->current.swayc_y - output->wlr_output->ly - 1,
		.width = con->current.swayc_width + 2,
		.height = con->current.swayc_height + 2,
	};
	scale_box(&box, output->wlr_output->scale);
	wlr_output_damage_add_box(output->damage, &box);
}

static void damage_handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_output *output =
		wl_container_of(listener, output, damage_destroy);
	container_destroy(output->swayc);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, destroy);
	wl_signal_emit(&output->events.destroy, output);

	if (output->swayc) {
		container_destroy(output->swayc);
	}

	wl_list_remove(&output->link);
	wl_list_remove(&output->destroy.link);
	output->wlr_output->data = NULL;
	free(output);

	arrange_windows(&root_container);
}

static void handle_mode(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, mode);
	arrange_layers(output);
	arrange_windows(output->swayc);
	transaction_commit_dirty();
}

static void handle_transform(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, transform);
	arrange_layers(output);
	arrange_windows(output->swayc);
	transaction_commit_dirty();
}

static void handle_scale_iterator(struct sway_container *view, void *data) {
	view_update_marks_textures(view->sway_view);
}

static void handle_scale(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, scale);
	arrange_layers(output);
	container_descendants(output->swayc, C_VIEW, handle_scale_iterator, NULL);
	arrange_windows(output->swayc);
	transaction_commit_dirty();
}

struct sway_output *output_from_wlr_output(struct wlr_output *wlr_output) {
	return wlr_output->data;
}

void handle_new_output(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;
	wlr_log(WLR_DEBUG, "New output %p: %s", wlr_output, wlr_output->name);

	struct sway_output *output = calloc(1, sizeof(struct sway_output));
	if (!output) {
		return;
	}
	output->wlr_output = wlr_output;
	wlr_output->data = output;
	output->server = server;
	output->damage = wlr_output_damage_create(wlr_output);

	wl_signal_add(&wlr_output->events.destroy, &output->destroy);
	output->destroy.notify = handle_destroy;

	wl_list_insert(&root_container.sway_root->outputs, &output->link);

	if (!wl_list_empty(&wlr_output->modes)) {
		struct wlr_output_mode *mode =
			wl_container_of(wlr_output->modes.prev, mode, link);
		wlr_output_set_mode(wlr_output, mode);
	}

	output_enable(output);
}

void output_enable(struct sway_output *output) {
	struct wlr_output *wlr_output = output->wlr_output;

	if (!sway_assert(output->swayc == NULL, "output is already enabled")) {
		return;
	}

	output->swayc = output_create(output);
	if (!output->swayc) {
		// Output is disabled
		return;
	}

	size_t len = sizeof(output->layers) / sizeof(output->layers[0]);
	for (size_t i = 0; i < len; ++i) {
		wl_list_init(&output->layers[i]);
	}
	wl_signal_init(&output->events.destroy);

	input_manager_configure_xcursor(input_manager);

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
	arrange_windows(&root_container);
	transaction_commit_dirty();
}
