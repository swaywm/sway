#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <wayland-server.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_surface.h>
#include <wlr/types/wlr_wl_shell.h>
#include <wlr/util/region.h>
#include "log.h"
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
	if (!wlr_surface_has_buffer(surface)) {
		return false;
	}

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

static void surface_for_each_surface(struct wlr_surface *surface,
		double ox, double oy, struct root_geometry *geo,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	geo->x = ox;
	geo->y = oy;
	geo->width = surface->current->width;
	geo->height = surface->current->height;
	geo->rotation = 0;

	wlr_surface_for_each_surface(surface, iterator, user_data);
}

static void output_view_for_each_surface(struct sway_view *view,
		struct root_geometry *geo, wlr_surface_iterator_func_t iterator,
		void *user_data) {
	geo->x = view->x;
	geo->y = view->y;
	geo->width = view->surface->current->width;
	geo->height = view->surface->current->height;
	geo->rotation = 0; // TODO

	view_for_each_surface(view, iterator, user_data);
}

static void layer_for_each_surface(struct wl_list *layer_surfaces,
		struct root_geometry *geo, wlr_surface_iterator_func_t iterator,
		void *user_data) {
	struct sway_layer_surface *layer_surface;
	wl_list_for_each(layer_surface, layer_surfaces, link) {
		struct wlr_layer_surface *wlr_layer_surface =
			layer_surface->layer_surface;
		surface_for_each_surface(wlr_layer_surface->surface,
			layer_surface->geo.x, layer_surface->geo.y, geo, iterator,
			user_data);
	}
}

static void unmanaged_for_each_surface(struct wl_list *unmanaged,
		struct sway_output *output, struct root_geometry *geo,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	struct sway_xwayland_unmanaged *unmanaged_surface;
	wl_list_for_each(unmanaged_surface, unmanaged, link) {
		struct wlr_xwayland_surface *xsurface =
			unmanaged_surface->wlr_xwayland_surface;
		double ox = unmanaged_surface->lx - output->swayc->x;
		double oy = unmanaged_surface->ly - output->swayc->y;

		surface_for_each_surface(xsurface->surface, ox, oy, geo,
			iterator, user_data);
	}
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
	float alpha;
};

static void render_surface_iterator(struct wlr_surface *surface, int sx, int sy,
		void *_data) {
	struct render_data *data = _data;
	struct wlr_output *wlr_output = data->output->wlr_output;
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
}

static void render_layer(struct sway_output *output,
		struct wl_list *layer_surfaces) {
	struct render_data data = { .output = output, .alpha = 1.0f };
	layer_for_each_surface(layer_surfaces, &data.root_geo,
		render_surface_iterator, &data);
}

static void render_unmanaged(struct sway_output *output,
		struct wl_list *unmanaged) {
	struct render_data data = { .output = output, .alpha = 1.0f };
	unmanaged_for_each_surface(unmanaged, output, &data.root_geo,
		render_surface_iterator, &data);
}

static void render_view(struct sway_view *view, struct sway_output *output) {
	struct render_data data = { .output = output, .alpha = view->swayc->alpha };
	output_view_for_each_surface(
			view, &data.root_geo, render_surface_iterator, &data);
}

/**
 * Select a color to use to render a border based on focus.
 */
static struct border_colors *select_focus_color(char *name) {
	struct border_colors *colors;
	if (strcmp(name, "focused") == 0) {
		return colors = &config->border_colors.focused;
	} else if (strcmp(name, "inactive") == 0) {
		return colors = &config->border_colors.focused_inactive;
	} else if (strcmp(name, "unfocused") == 0) {
		return colors =&config->border_colors.unfocused;
	} else {
		return NULL;
	}
}

/**
 * Get all title textures from children of a parent.
 */
static struct wlr_texture **get_title_textures(struct sway_container *con) { 
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focus(seat);
	int num_tabs = con->children->length;
	struct wlr_texture **title_texture = 
		malloc(num_tabs * sizeof(struct wlr_texture));

	for (int i = 0; i < con->children->length; ++i) {
		struct sway_container *child = con->children->items[i];

		if (child->type == C_VIEW) {
			if (child->sway_view->border != B_NONE) {
				if (focus == child) {
					title_texture[i] = child->title_focused;
				} else if (seat_get_focus_inactive(seat, con) == child) {
					title_texture[i] = child->title_focused_inactive;
				} else {
					title_texture[i] = child->title_unfocused;
				}
			}
		}
	}
	return title_texture;
}

/**
 * Render decorations for the top border with "border normal".
 */
static void render_container_top_border_normal(struct sway_output *output,
		struct sway_container *con, struct border_colors *colors,
		struct wlr_texture *title_texture, int depth) {

	struct wlr_renderer *renderer =
		wlr_backend_get_renderer(output->wlr_output->backend);
	struct wlr_box box;
	float color[4];
	size_t num_tabs = 1;
	size_t tab_width = 1;
	
	if (con->parent->layout == L_TABBED) {
		num_tabs = con->parent->children->length;
		tab_width = (con->width / num_tabs);
	}

	// Single pixel bar above title
	memcpy(&color, colors->border, sizeof(float) * 4);
	color[3] *= con->alpha;
	box.x = con->x + depth * tab_width;
	box.y = con->y;
	box.width = con->width / num_tabs;
	box.height = 1;
	scale_box(&box, output->wlr_output->scale);
	wlr_render_rect(renderer, &box, color,
			output->wlr_output->transform_matrix);

	// Single pixel bar below title
	box.x = con->x + depth * tab_width;
	box.y = con->sway_view->y - 1;
	box.width = con->width / num_tabs;
	box.height = 1;
	scale_box(&box, output->wlr_output->scale);
	wlr_render_rect(renderer, &box, color,
			output->wlr_output->transform_matrix);

	// Title background
	memcpy(&color, colors->background, sizeof(float) * 4);
	color[3] *= con->alpha;
	box.x = con->x + depth * tab_width;
	box.y = con->y + 1;
	box.width = con->width / num_tabs + 2;
	box.height = con->sway_view->y - con->y - 2;
	scale_box(&box, output->wlr_output->scale);
	wlr_render_rect(renderer, &box, color,
			output->wlr_output->transform_matrix);

	// Title text
	if (title_texture) {
		wlr_renderer_scissor(renderer, &box);
		wlr_render_texture(renderer, title_texture,
				output->wlr_output->transform_matrix, box.x + 2, box.y, 1);
		wlr_renderer_scissor(renderer, NULL);
	}
}

/**
 * Render decorations for the top border for tabbed view with "border normal".
 */
static void render_container_top_tabbed_border_normal(struct sway_output *output,
		struct sway_container *con, struct wlr_texture **title_texture,
		int focus_number, bool active) {

	int index_tabs = con->parent->children->length - 1;
	int index_left_of_focus = focus_number - 1;
	struct border_colors *unfocused = select_focus_color("unfocused");
	struct border_colors *focus_color;

	if (active) {
		focus_color = select_focus_color("focused");
	} else {
		focus_color = select_focus_color("inactive");
	}

	if (index_left_of_focus >= 0) {
		for (int i = 0; i <= index_left_of_focus; ++i) {
			render_container_top_border_normal(output, con, unfocused, title_texture[i], i);
		}
	}
	render_container_top_border_normal(output, con, focus_color, title_texture[focus_number], focus_number);
	if (index_tabs > index_left_of_focus) {
		for (int j = focus_number + 1; j <= index_tabs; ++j) {
			render_container_top_border_normal(output, con, unfocused, title_texture[j], j);
		}
	}
}

/**
 * Render decorations for a horizontal or vertical view with "border normal".
 */
static void render_container_simple_border_normal(struct sway_output *output,
		struct sway_container *con, struct border_colors *colors,
		struct wlr_texture *title_texture, bool top) {

	struct wlr_renderer *renderer =
		wlr_backend_get_renderer(output->wlr_output->backend);
	struct wlr_box box;
	float color[4];

	// Child border - left edge
	memcpy(&color, colors->child_border, sizeof(float) * 4);
	color[3] *= con->alpha;
	box.x = con->x;
	box.y = con->y + 1;
	box.width = con->sway_view->border_thickness;
	box.height = con->height - 1;
	scale_box(&box, output->wlr_output->scale);
	wlr_render_rect(renderer, &box, color,
			output->wlr_output->transform_matrix);

	// Child border - right edge
	if (con->parent->children->length == 1 && con->parent->layout == L_HORIZ) {
		memcpy(&color, colors->indicator, sizeof(float) * 4);
	} else {
		memcpy(&color, colors->child_border, sizeof(float) * 4);
	}
	color[3] *= con->alpha;
	box.x = con->x + con->width - con->sway_view->border_thickness;
	box.y = con->y + 1;
	box.width = con->sway_view->border_thickness;
	box.height = con->height - 1;
	scale_box(&box, output->wlr_output->scale);
	wlr_render_rect(renderer, &box, color,
			output->wlr_output->transform_matrix);

	// Child border - bottom edge
	if (con->parent->children->length == 1 && con->parent->layout == L_VERT) {
		memcpy(&color, colors->indicator, sizeof(float) * 4);
	} else {
		memcpy(&color, colors->child_border, sizeof(float) * 4);
	}
	color[3] *= con->alpha;
	box.x = con->x;
	box.y = con->y + con->height - con->sway_view->border_thickness;
	box.width = con->width;
	box.height = con->sway_view->border_thickness;
	scale_box(&box, output->wlr_output->scale);
	wlr_render_rect(renderer, &box, color,
			output->wlr_output->transform_matrix);

	if (top) {
		render_container_top_border_normal(output, con, colors, title_texture, 0);
	}
}

/**
 * Render decorations for a view with "border pixel".
 */
static void render_container_simple_border_pixel(struct sway_output *output,
		struct sway_container *con, struct border_colors *colors) {
	struct wlr_renderer *renderer =
		wlr_backend_get_renderer(output->wlr_output->backend);
	struct wlr_box box;
	float color[4];

	memcpy(&color, colors->child_border, sizeof(float) * 4);
	color[3] *= con->alpha;
	box.x = con->x;
	box.y = con->y;
	box.width = con->sway_view->border_thickness;
	box.height = con->height;
	scale_box(&box, output->wlr_output->scale);
	wlr_render_rect(renderer, &box, color,
			output->wlr_output->transform_matrix);

	// Child border - right edge
	if (con->parent->children->length == 1 && con->parent->layout == L_HORIZ) {
		memcpy(&color, colors->indicator, sizeof(float) * 4);
	} else {
		memcpy(&color, colors->child_border, sizeof(float) * 4);
	}
	color[3] *= con->alpha;
	box.x = con->x + con->width - con->sway_view->border_thickness;
	box.y = con->y;
	box.width = con->sway_view->border_thickness;
	box.height = con->height;
	scale_box(&box, output->wlr_output->scale);
	wlr_render_rect(renderer, &box, color,
			output->wlr_output->transform_matrix);

	// Child border - top edge
	box.x = con->x;
	box.y = con->y;
	box.width = con->width;
	box.height = con->sway_view->border_thickness;
	scale_box(&box, output->wlr_output->scale);
	wlr_render_rect(renderer, &box, color,
			output->wlr_output->transform_matrix);

	// Child border - bottom edge
	if (con->parent->children->length == 1 && con->parent->layout == L_VERT) {
		memcpy(&color, colors->indicator, sizeof(float) * 4);
	} else {
		memcpy(&color, colors->child_border, sizeof(float) * 4);
	}
	color[3] *= con->alpha;
	box.x = con->x;
	box.y = con->y + con->height - con->sway_view->border_thickness;
	box.width = con->width;
	box.height = con->sway_view->border_thickness;
	scale_box(&box, output->wlr_output->scale);
	wlr_render_rect(renderer, &box, color,
			output->wlr_output->transform_matrix);
}

/**
 * Render decorations for a tabbed view with "border normal".
 */

static void render_container_tabbed_border_normal(struct sway_output *output,
		struct sway_container *con, struct wlr_texture **title_texture,
		int depth, bool active) {

	struct border_colors *focused_color = select_focus_color("focused");
	render_container_simple_border_normal(output, con, focused_color,
			NULL, false);
	render_container_top_tabbed_border_normal(output, con, title_texture,
			depth, active);
}
static void render_container(struct sway_output *output,
		struct sway_container *con);

/**
 * Render a container's children using a L_HORIZ or L_VERT layout.
 *
 * Wrap child views in borders and leave child containers borderless because
 * they'll apply their own borders to their children.
 */
static void render_container_simple(struct sway_output *output,
		struct sway_container *con) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focus(seat);

	for (int i = 0; i < con->children->length; ++i) {
		struct sway_container *child = con->children->items[i];

		if (child->type == C_VIEW) {
			if (child->sway_view->border != B_NONE) {
				struct border_colors *colors;
				struct wlr_texture *title_texture;
				if (focus == child) {
					colors = select_focus_color("focused");
					title_texture = child->title_focused;
				} else if (seat_get_focus_inactive(seat, con) == child) {
					colors = select_focus_color("inactive");
					title_texture = child->title_focused_inactive;
				} else {
					colors = select_focus_color("unfocused");
					title_texture = child->title_unfocused;
				}

				if (child->sway_view->border == B_NORMAL) {
					render_container_simple_border_normal(output, child,
							colors, title_texture, true);
				} else {
					render_container_simple_border_pixel(output, child, colors);
				}
			}
			render_view(child->sway_view, output);
		} else {
			render_container(output, child);
		}
	}
}

/**
 * Render a container's children using the L_TABBED layout.
 */
static void render_container_tabbed(struct sway_output *output,
		struct sway_container *con) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focus(seat);
	struct wlr_texture **title_texture = get_title_textures(con);

	for (int i = 0; i < con->children->length; ++i) {
		struct sway_container *child = con->children->items[i];

		if (child->type == C_VIEW) {
			if (child->sway_view->border != B_NONE) {
				if (focus == child) {
					render_container_tabbed_border_normal(output, child,
							title_texture, i, true);
					render_view(child->sway_view, output);
				} else if (seat_get_focus_inactive(seat, con) == child) {
					render_container_tabbed_border_normal(output, child,
							title_texture, i, false);
					render_view(child->sway_view, output);
				}
			}
		} else {
			render_container(output, child);
		}
	}
}

/**
 * Render a container's children using the L_STACKED layout.
 */
static void render_container_stacked(struct sway_output *output,
		struct sway_container *con) {
	// TODO
}

static void render_container(struct sway_output *output,
		struct sway_container *con) {
	switch (con->layout) {
	case L_NONE:
	case L_HORIZ:
	case L_VERT:
		render_container_simple(output, con);
		break;
	case L_STACKED:
		render_container_stacked(output, con);
		break;
	case L_TABBED:
		render_container_tabbed(output, con);
		break;
	case L_FLOATING:
		// TODO
		break;
	}
}

static struct sway_container *output_get_active_workspace(
		struct sway_output *output) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus =
		seat_get_focus_inactive(seat, output->swayc);
	if (!focus) {
		// We've never been to this output before
		focus = output->swayc->children->items[0];
	}
	struct sway_container *workspace = focus;
	if (workspace->type != C_WORKSPACE) {
		workspace = container_parent(workspace, C_WORKSPACE);
	}
	return workspace;
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

	// TODO: don't damage the whole output
	int width, height;
	wlr_output_transformed_resolution(wlr_output, &width, &height);
	pixman_region32_union_rect(damage, damage, 0, 0, width, height);

	struct sway_container *workspace = output_get_active_workspace(output);

	if (workspace->sway_workspace->fullscreen) {
		float clear_color[] = {0.0f, 0.0f, 0.0f, 1.0f};
		wlr_renderer_clear(renderer, clear_color);
		// TODO: handle views smaller than the output
		render_view(workspace->sway_workspace->fullscreen, output);

		if (workspace->sway_workspace->fullscreen->type == SWAY_VIEW_XWAYLAND) {
			render_unmanaged(output,
					&root_container.sway_root->xwayland_unmanaged);
		}
	} else {
		float clear_color[] = {0.25f, 0.25f, 0.25f, 1.0f};
		wlr_renderer_clear(renderer, clear_color);

		render_layer(output,
				&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
		render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

		render_container(output, workspace);

		render_unmanaged(output, &root_container.sway_root->xwayland_unmanaged);
		render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
	}
	render_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);

renderer_end:
	if (root_container.sway_root->debug_tree) {
		wlr_render_texture(renderer, root_container.sway_root->debug_tree,
			wlr_output->transform_matrix, 0, 0, 1);
	}

	wlr_renderer_end(renderer);
	if (!wlr_output_damage_swap_buffers(output->damage, when, damage)) {
		return;
	}
	output->last_frame = *when;
}

struct send_frame_done_data {
	struct root_geometry root_geo;
	struct sway_output *output;
	struct timespec *when;
};

static void send_frame_done_iterator(struct wlr_surface *surface,
		int sx, int sy, void *_data) {
	struct send_frame_done_data *data = _data;

	bool intersects = get_surface_box(&data->root_geo, data->output, surface,
		sx, sy, NULL);
	if (intersects) {
		wlr_surface_send_frame_done(surface, data->when);
	}
}

static void send_frame_done_layer(struct send_frame_done_data *data,
		struct wl_list *layer_surfaces) {
	layer_for_each_surface(layer_surfaces, &data->root_geo,
		send_frame_done_iterator, data);
}

static void send_frame_done_unmanaged(struct send_frame_done_data *data,
		struct wl_list *unmanaged) {
	unmanaged_for_each_surface(unmanaged, data->output, &data->root_geo,
		send_frame_done_iterator, data);
}

static void send_frame_done_container_iterator(struct sway_container *con,
		void *_data) {
	struct send_frame_done_data *data = _data;
	if (!sway_assert(con->type == C_VIEW, "expected a view")) {
		return;
	}
	output_view_for_each_surface(con->sway_view, &data->root_geo,
		send_frame_done_iterator, data);
}

static void send_frame_done_container(struct send_frame_done_data *data,
		struct sway_container *con) {
	container_descendants(con, C_VIEW,
		send_frame_done_container_iterator, data);
}

static void send_frame_done(struct sway_output *output, struct timespec *when) {
	struct send_frame_done_data data = {
		.output = output,
		.when = when,
	};

	send_frame_done_layer(&data,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
	send_frame_done_layer(&data,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

	struct sway_container *workspace = output_get_active_workspace(output);
	send_frame_done_container(&data, workspace);

	send_frame_done_unmanaged(&data,
		&root_container.sway_root->xwayland_unmanaged);

	send_frame_done_layer(&data,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
	send_frame_done_layer(&data,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);
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
	bool intersects = get_surface_box(&data->root_geo, data->output, surface,
		sx, sy, &box);
	if (!intersects) {
		return;
	}

	scale_box(&box, output->wlr_output->scale);

	if (whole) {
		wlr_box_rotated_bounds(&box, rotation, &box);
		wlr_output_damage_add_box(output->damage, &box);
	} else {
		int center_x = box.x + box.width/2;
		int center_y = box.y + box.height/2;

		pixman_region32_t damage;
		pixman_region32_init(&damage);
		pixman_region32_copy(&damage, &surface->current->surface_damage);
		wlr_region_scale(&damage, &damage, output->wlr_output->scale);
		if (ceil(output->wlr_output->scale) > surface->current->scale) {
			// When scaling up a surface, it'll become blurry so we need to
			// expand the damage region
			wlr_region_expand(&damage, &damage,
				ceil(output->wlr_output->scale) - surface->current->scale);
		}
		pixman_region32_translate(&damage, box.x, box.y);
		wlr_region_rotated_bounds(&damage, &damage, rotation,
			center_x, center_y);
		wlr_output_damage_add(output->damage, &damage);
		pixman_region32_fini(&damage);
	}
}

void output_damage_surface(struct sway_output *output, double ox, double oy,
		struct wlr_surface *surface, bool whole) {
	struct damage_data data = {
		.output = output,
		.whole = whole,
	};

	surface_for_each_surface(surface, ox, oy, &data.root_geo,
		damage_surface_iterator, &data);
}

void output_damage_view(struct sway_output *output, struct sway_view *view,
		bool whole) {
	if (!sway_assert(view->swayc != NULL, "expected a view in the tree")) {
		return;
	}

	struct sway_container *workspace = container_parent(view->swayc,
			C_WORKSPACE);
	if (workspace->sway_workspace->fullscreen && !view->is_fullscreen) {
		return;
	}

	struct damage_data data = {
		.output = output,
		.whole = whole,
	};

	output_view_for_each_surface(view, &data.root_geo,
		damage_surface_iterator, &data);
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
	float scale = output->wlr_output->scale;
	struct wlr_box box = {
		.x = con->x * scale,
		.y = con->y * scale,
		.width = con->width * scale,
		.height = con->height * scale,
	};
	wlr_output_damage_add_box(output->damage, &box);

	container_descendants(con, C_VIEW, output_damage_whole_container_iterator,
		output);
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
	arrange_output(output->swayc);
}

static void handle_transform(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, transform);
	arrange_layers(output);
	arrange_output(output->swayc);
}

static void handle_scale(struct wl_listener *listener, void *data) {
	struct sway_output *output = wl_container_of(listener, output, scale);
	arrange_layers(output);
	arrange_output(output->swayc);
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
	arrange_root();
}
