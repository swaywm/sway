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
#include "sway/desktop/render.h"
#include "sway/desktop/transaction.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

struct render_context {
	struct sway_output *output;
	double output_lx;
	double output_ly;
	pixman_region32_t *damage;
	float scale;
};
struct render_context context;

static void scale_box(struct wlr_box *box, float scale) {
	box->x *= scale;
	box->y *= scale;
	box->width *= scale;
	box->height *= scale;
}

static void scissor_output(struct wlr_output *wlr_output,
		pixman_box32_t *rect) {
	struct wlr_renderer *renderer = wlr_backend_get_renderer(wlr_output->backend);
	assert(renderer);

	struct wlr_box box = {
		.x = rect->x1,
		.y = rect->y1,
		.width = rect->x2 - rect->x1,
		.height = rect->y2 - rect->y1,
	};

	int ow, oh;
	wlr_output_transformed_resolution(wlr_output, &ow, &oh);

	enum wl_output_transform transform =
		wlr_output_transform_invert(wlr_output->transform);
	wlr_box_transform(&box, transform, ow, oh, &box);

	wlr_renderer_scissor(renderer, &box);
}

static void render_texture(struct wlr_texture *texture,
		const struct wlr_box *box, const float matrix[static 9], float alpha) {
	struct wlr_output *wlr_output = context.output->wlr_output;
	struct wlr_renderer *renderer =
		wlr_backend_get_renderer(wlr_output->backend);

	pixman_region32_t damage;
	pixman_region32_init(&damage);
	pixman_region32_union_rect(&damage, &damage, box->x, box->y,
		box->width, box->height);
	pixman_region32_intersect(&damage, &damage, context.damage);
	bool damaged = pixman_region32_not_empty(&damage);
	if (!damaged) {
		goto damage_finish;
	}

	int nrects;
	pixman_box32_t *rects = pixman_region32_rectangles(&damage, &nrects);
	for (int i = 0; i < nrects; ++i) {
		scissor_output(wlr_output, &rects[i]);
		wlr_render_texture_with_matrix(renderer, texture, matrix, alpha);
	}

damage_finish:
	pixman_region32_fini(&damage);
}

static void render_surface(struct wlr_surface *surface, int ox, int oy,
		float alpha) {
	struct wlr_output *wlr_output = context.output->wlr_output;

	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (!texture) {
		return;
	}

	struct wlr_box surface_box = {
		.x = ox,
		.y = oy,
		.width = surface->current->width,
		.height = surface->current->height,
	};
	struct wlr_box output_box = {
		.width = context.output->swayc->current.swayc_width,
		.height = context.output->swayc->current.swayc_height,
	};
	struct wlr_box intersection;
	if (!wlr_box_intersection(&output_box, &surface_box, &intersection)) {
		return;
	}

	scale_box(&surface_box, context.scale);

	float matrix[9];
	enum wl_output_transform transform =
		wlr_output_transform_invert(surface->current->transform);
	wlr_matrix_project_box(matrix, &surface_box, transform, 0,
		wlr_output->transform_matrix);

	render_texture(texture, &surface_box, matrix, alpha);
}

static void render_surfaces(struct wlr_surface *surface, double ox, double oy,
		float alpha) {
	render_surface(surface, ox, oy, alpha);

	struct wlr_subsurface *subsurface;
	wl_list_for_each(subsurface, &surface->subsurfaces, parent_link) {
		struct wlr_surface_state *state = subsurface->surface->current;
		int sx = state->subsurface_position.x;
		int sy = state->subsurface_position.y;

		render_surfaces(subsurface->surface, ox + sx, oy + sy, alpha);
	}
}

static void render_layer(struct wl_list *layer_surfaces) {
	struct sway_layer_surface *layer_surface;
	wl_list_for_each(layer_surface, layer_surfaces, link) {
		struct wlr_layer_surface *wlr_layer_surface =
			layer_surface->layer_surface;
		render_surfaces(wlr_layer_surface->surface,
				layer_surface->geo.x, layer_surface->geo.y, 1.0f);
	}
}

static void render_unmanaged(struct wl_list *unmanaged) {
	struct sway_xwayland_unmanaged *unmanaged_surface;
	wl_list_for_each(unmanaged_surface, unmanaged, link) {
		struct wlr_xwayland_surface *xsurface =
			unmanaged_surface->wlr_xwayland_surface;
		double ox = unmanaged_surface->lx - context.output_lx;
		double oy = unmanaged_surface->ly - context.output_ly;

		render_surfaces(xsurface->surface, ox, oy, 1.0f);
	}
}

static void render_drag_icons(struct wl_list *drag_icons) {
	struct sway_drag_icon *drag_icon;
	wl_list_for_each(drag_icon, drag_icons, link) {
		double ox = drag_icon->x - context.output_lx;
		double oy = drag_icon->y - context.output_ly;

		if (drag_icon->wlr_drag_icon->mapped) {
			render_surfaces(drag_icon->wlr_drag_icon->surface, ox, oy, 1.0f);
		}
	}
}

static void render_rect(const struct wlr_box *_box, float color[static 4]) {
	struct wlr_output *wlr_output = context.output->wlr_output;
	struct wlr_renderer *renderer =
		wlr_backend_get_renderer(wlr_output->backend);

	struct wlr_box box;
	memcpy(&box, _box, sizeof(struct wlr_box));
	box.x -= context.output_lx * context.scale;
	box.y -= context.output_ly * context.scale;

	pixman_region32_t damage;
	pixman_region32_init(&damage);
	pixman_region32_union_rect(&damage, &damage, box.x, box.y,
		box.width, box.height);
	pixman_region32_intersect(&damage, &damage, context.damage);
	bool damaged = pixman_region32_not_empty(&damage);
	if (!damaged) {
		goto damage_finish;
	}

	int nrects;
	pixman_box32_t *rects = pixman_region32_rectangles(&damage, &nrects);
	for (int i = 0; i < nrects; ++i) {
		scissor_output(wlr_output, &rects[i]);
		wlr_render_rect(renderer, &box, color, wlr_output->transform_matrix);
	}

damage_finish:
	pixman_region32_fini(&damage);
}

static void premultiply_alpha(float color[4], float opacity) {
	color[3] *= opacity;
	color[0] *= color[3];
	color[1] *= color[3];
	color[2] *= color[3];
}

static void render_saved_view(struct sway_view *view, float alpha) {
	int width, height;
	struct wlr_texture *texture =
		transaction_get_saved_texture(view, &width, &height);
	if (!texture) {
		return;
	}
	struct wlr_box box = {
		.x = view->swayc->current.view_x - context.output_lx,
		.y = view->swayc->current.view_y - context.output_ly,
		.width = width,
		.height = height,
	};

	struct wlr_box output_box = {
		.width = context.output->swayc->current.swayc_width,
		.height = context.output->swayc->current.swayc_height,
	};

	struct wlr_box intersection;
	bool intersects = wlr_box_intersection(&output_box, &box, &intersection);
	if (!intersects) {
		return;
	}

	scale_box(&box, context.scale);

	float matrix[9];
	wlr_matrix_project_box(matrix, &box, WL_OUTPUT_TRANSFORM_NORMAL, 0,
		context.output->wlr_output->transform_matrix);

	render_texture(texture, &box, matrix, alpha);
}

/**
 * Render a view's surface and left/bottom/right borders.
 */
static void render_view(struct sway_container *con,
		struct border_colors *colors) {
	struct sway_view *view = con->sway_view;
	if (view->swayc->instructions->length) {
		render_saved_view(view, view->swayc->alpha);
	} else {
		double ox = view->swayc->current.view_x - context.output_lx;
		double oy = view->swayc->current.view_y - context.output_ly;
		render_surfaces(view->surface, ox, oy, view->swayc->alpha);
	}

	struct wlr_box box;
	float color[4];
	struct sway_container_state *state = &con->current;

	if (state->border != B_NONE) {
		if (state->border_left) {
			memcpy(&color, colors->child_border, sizeof(float) * 4);
			premultiply_alpha(color, con->alpha);
			box.x = state->swayc_x;
			box.y = state->view_y;
			box.width = state->border_thickness;
			box.height = state->view_height;
			scale_box(&box, context.scale);
			render_rect(&box, color);
		}

		if (state->border_right) {
			if (state->parent->current.children->length == 1
					&& state->parent->current.layout == L_HORIZ) {
				memcpy(&color, colors->indicator, sizeof(float) * 4);
			} else {
				memcpy(&color, colors->child_border, sizeof(float) * 4);
			}
			premultiply_alpha(color, con->alpha);
			box.x = state->view_x + state->view_width;
			box.y = state->view_y;
			box.width = state->border_thickness;
			box.height = state->view_height;
			scale_box(&box, context.scale);
			render_rect(&box, color);
		}

		if (state->border_bottom) {
			if (state->parent->current.children->length == 1
					&& con->current.parent->current.layout == L_VERT) {
				memcpy(&color, colors->indicator, sizeof(float) * 4);
			} else {
				memcpy(&color, colors->child_border, sizeof(float) * 4);
			}
			premultiply_alpha(color, con->alpha);
			box.x = state->swayc_x;
			box.y = state->view_y + state->view_height;
			box.width = state->swayc_width;
			box.height = state->border_thickness;
			scale_box(&box, context.scale);
			render_rect(&box, color);
		}
	}
}

/**
 * Render a titlebar.
 *
 * Care must be taken not to render over the same pixel multiple times,
 * otherwise the colors will be incorrect when using opacity.
 *
 * The height is: 1px border, 3px padding, font height, 3px padding, 1px border
 * The left side for L_TABBED is: 1px border, 2px padding, title
 * The left side for other layouts is: 3px padding, title
 */
static void render_titlebar(struct sway_container *con, int x, int y, int width,
		struct border_colors *colors, struct wlr_texture *title_texture,
		struct wlr_texture *marks_texture) {
	struct wlr_box box;
	float color[4];
	struct sway_container_state *state = &con->current;
	enum sway_container_layout layout = state->parent->current.layout;
	list_t *children = state->parent->current.children;
	bool is_last_child = children->items[children->length - 1] == con;

	// Single pixel bar above title
	memcpy(&color, colors->border, sizeof(float) * 4);
	premultiply_alpha(color, con->alpha);
	box.x = x;
	box.y = y;
	box.width = width;
	box.height = TITLEBAR_BORDER_THICKNESS;
	scale_box(&box, context.scale);
	render_rect(&box, color);

	// Single pixel bar below title
	size_t left_offset = 0, right_offset = 0;
	bool connects_sides = false;
	if (layout == L_HORIZ || layout == L_VERT ||
			(layout == L_STACKED && is_last_child)) {
		if (con->type == C_VIEW) {
			left_offset = state->border_left * state->border_thickness;
			right_offset = state->border_right * state->border_thickness;
			connects_sides = true;
		}
	}
	box.x = x + left_offset;
	box.y = y + container_titlebar_height() - TITLEBAR_BORDER_THICKNESS;
	box.width = width - left_offset - right_offset;
	box.height = TITLEBAR_BORDER_THICKNESS;
	scale_box(&box, context.scale);
	render_rect(&box, color);

	if (layout == L_TABBED) {
		// Single pixel left edge
		box.x = x;
		box.y = y + TITLEBAR_BORDER_THICKNESS;
		box.width = TITLEBAR_BORDER_THICKNESS;
		box.height =
			container_titlebar_height() - TITLEBAR_BORDER_THICKNESS * 2;
		scale_box(&box, context.scale);
		render_rect(&box, color);

		// Single pixel right edge
		box.x = (x + width - TITLEBAR_BORDER_THICKNESS) * context.scale;
		render_rect(&box, color);
	}

	size_t inner_width = width - TITLEBAR_H_PADDING * 2;

	// Marks
	size_t marks_width = 0;
	if (config->show_marks && marks_texture) {
		struct wlr_box texture_box;
		wlr_texture_get_size(marks_texture,
			&texture_box.width, &texture_box.height);
		texture_box.x = (x - context.output_lx + width - TITLEBAR_H_PADDING)
			* context.scale - texture_box.width;
		texture_box.y = (y - context.output_ly + TITLEBAR_V_PADDING)
			* context.scale;

		float matrix[9];
		wlr_matrix_project_box(matrix, &texture_box,
			WL_OUTPUT_TRANSFORM_NORMAL,
			0.0, context.output->wlr_output->transform_matrix);

		if (inner_width * context.scale < texture_box.width) {
			texture_box.width = inner_width * context.scale;
		}
		render_texture(marks_texture, &texture_box, matrix, con->alpha);
		marks_width = texture_box.width;
	}

	// Title text
	size_t title_width = 0;
	if (title_texture) {
		struct wlr_box texture_box;
		wlr_texture_get_size(title_texture,
			&texture_box.width, &texture_box.height);
		texture_box.x =
			(x - context.output_lx + TITLEBAR_H_PADDING) * context.scale;
		texture_box.y =
			(y - context.output_ly + TITLEBAR_V_PADDING) * context.scale;

		float matrix[9];
		wlr_matrix_project_box(matrix, &texture_box,
			WL_OUTPUT_TRANSFORM_NORMAL,
			0.0, context.output->wlr_output->transform_matrix);

		if (inner_width * context.scale - marks_width < texture_box.width) {
			texture_box.width = inner_width * context.scale - marks_width;
		}
		render_texture(title_texture, &texture_box, matrix, con->alpha);
		title_width = texture_box.width;
	}

	// Padding above title
	memcpy(&color, colors->background, sizeof(float) * 4);
	premultiply_alpha(color, con->alpha);
	box.x = x + (layout == L_TABBED) * TITLEBAR_BORDER_THICKNESS;
	box.y = y + TITLEBAR_BORDER_THICKNESS;
	box.width = width - (layout == L_TABBED) * TITLEBAR_BORDER_THICKNESS * 2;
	box.height = TITLEBAR_V_PADDING - TITLEBAR_BORDER_THICKNESS;
	scale_box(&box, context.scale);
	render_rect(&box, color);

	// Padding below title
	box.y = (y + TITLEBAR_V_PADDING + config->font_height) * context.scale;
	render_rect(&box, color);

	// Filler between title and marks
	box.width = inner_width * context.scale - title_width - marks_width;
	if (box.width > 0) {
		box.x = (x + TITLEBAR_H_PADDING) * context.scale + title_width;
		box.y = (y + TITLEBAR_V_PADDING) * context.scale;
		box.height = config->font_height * context.scale;
		render_rect(&box, color);
	}

	// Padding left of title
	left_offset = (layout == L_TABBED) * TITLEBAR_BORDER_THICKNESS;
	box.x = x + left_offset;
	box.y = y + TITLEBAR_V_PADDING;
	box.width = TITLEBAR_H_PADDING - left_offset;
	box.height = config->font_height;
	scale_box(&box, context.scale);
	render_rect(&box, color);

	// Padding right of marks
	right_offset = (layout == L_TABBED) * TITLEBAR_BORDER_THICKNESS;
	box.x = x + width - TITLEBAR_H_PADDING;
	box.y = y + TITLEBAR_V_PADDING;
	box.width = TITLEBAR_H_PADDING - right_offset;
	box.height = config->font_height;
	scale_box(&box, context.scale);
	render_rect(&box, color);

	if (connects_sides) {
		// Left pixel in line with bottom bar
		box.x = x;
		box.y = y + container_titlebar_height() - TITLEBAR_BORDER_THICKNESS;
		box.width = state->border_thickness * state->border_left;
		box.height = TITLEBAR_BORDER_THICKNESS;
		scale_box(&box, context.scale);
		render_rect(&box, color);

		// Right pixel in line with bottom bar
		box.x = x + width - state->border_thickness * state->border_right;
		box.y = y + container_titlebar_height() - TITLEBAR_BORDER_THICKNESS;
		box.width = state->border_thickness * state->border_right;
		box.height = TITLEBAR_BORDER_THICKNESS;
		scale_box(&box, context.scale);
		render_rect(&box, color);
	}
}

/**
 * Render the top border line for a view using "border pixel".
 */
static void render_top_border(struct sway_container *con,
		struct border_colors *colors) {
	struct sway_container_state *state = &con->current;
	if (!state->border_top) {
		return;
	}
	struct wlr_box box;
	float color[4];

	// Child border - top edge
	memcpy(&color, colors->child_border, sizeof(float) * 4);
	premultiply_alpha(color, con->alpha);
	box.x = state->swayc_x;
	box.y = state->swayc_y;
	box.width = state->swayc_width;
	box.height = state->border_thickness;
	scale_box(&box, context.scale);
	render_rect(&box, color);
}

static void render_container(struct sway_container *con, bool parent_focused);

/**
 * Render a container's children using a L_HORIZ or L_VERT layout.
 *
 * Wrap child views in borders and leave child containers borderless because
 * they'll apply their own borders to their children.
 */
static void render_container_simple(struct sway_container *con,
		bool parent_focused) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focus(seat);

	for (int i = 0; i < con->current.children->length; ++i) {
		struct sway_container *child = con->current.children->items[i];

		if (child->type == C_VIEW) {
			struct sway_view *view = child->sway_view;
			struct border_colors *colors;
			struct wlr_texture *title_texture;
			struct wlr_texture *marks_texture;
			struct sway_container_state *state = &child->current;

			if (focus == child || parent_focused) {
				colors = &config->border_colors.focused;
				title_texture = child->title_focused;
				marks_texture = view->marks_focused;
			} else if (seat_get_focus_inactive(seat, con) == child) {
				colors = &config->border_colors.focused_inactive;
				title_texture = child->title_focused_inactive;
				marks_texture = view->marks_focused_inactive;
			} else {
				colors = &config->border_colors.unfocused;
				title_texture = child->title_unfocused;
				marks_texture = view->marks_unfocused;
			}

			if (state->border == B_NORMAL) {
				render_titlebar(child, state->swayc_x, state->swayc_y,
						state->swayc_width, colors,
						title_texture, marks_texture);
			} else {
				render_top_border(child, colors);
			}
			render_view(child, colors);
		} else {
			render_container(child, parent_focused || focus == child);
		}
	}
}

/**
 * Render a container's children using the L_TABBED layout.
 */
static void render_container_tabbed(struct sway_container *con,
		bool parent_focused) {
	if (!con->current.children->length) {
		return;
	}
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focus(seat);
	struct sway_container *current = seat_get_active_current_child(seat, con);
	struct border_colors *current_colors = &config->border_colors.unfocused;
	struct sway_container_state *pstate = &con->current;

	// Render tabs
	for (int i = 0; i < con->current.children->length; ++i) {
		struct sway_container *child = con->current.children->items[i];
		struct sway_view *view = child->type == C_VIEW ? child->sway_view : NULL;
		struct sway_container_state *cstate = &child->current;
		struct border_colors *colors;
		struct wlr_texture *title_texture;
		struct wlr_texture *marks_texture;

		if (focus == child || parent_focused) {
			colors = &config->border_colors.focused;
			title_texture = child->title_focused;
			marks_texture = view ? view->marks_focused : NULL;
		} else if (child == current) {
			colors = &config->border_colors.focused_inactive;
			title_texture = child->title_focused_inactive;
			marks_texture = view ? view->marks_focused_inactive : NULL;
		} else {
			colors = &config->border_colors.unfocused;
			title_texture = child->title_unfocused;
			marks_texture = view ? view->marks_unfocused : NULL;
		}

		int tab_width = pstate->swayc_width / pstate->children->length;
		int x = pstate->swayc_x + tab_width * i;
		// Make last tab use the remaining width of the parent
		if (i == pstate->children->length - 1) {
			tab_width = pstate->swayc_width - tab_width * i;
		}

		render_titlebar(child, x, cstate->swayc_y, tab_width, colors,
				title_texture, marks_texture);

		if (child == current) {
			current_colors = colors;
		}
	}

	// Render surface and left/right/bottom borders
	if (current) {
		if (current->type == C_VIEW) {
			render_view(current, current_colors);
		} else {
			render_container(current, parent_focused || current == focus);
		}
	}
}

/**
 * Render a container's children using the L_STACKED layout.
 */
static void render_container_stacked(struct sway_container *con,
		bool parent_focused) {
	if (!con->current.children->length) {
		return;
	}
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focus(seat);
	struct sway_container *current = seat_get_active_current_child(seat, con);
	struct border_colors *current_colors = &config->border_colors.unfocused;
	struct sway_container_state *pstate = &con->current;

	// Render titles
	for (int i = 0; i < con->current.children->length; ++i) {
		struct sway_container *child = con->current.children->items[i];
		struct sway_view *view = child->type == C_VIEW ? child->sway_view : NULL;
		struct sway_container_state *cstate = &child->current;
		struct border_colors *colors;
		struct wlr_texture *title_texture;
		struct wlr_texture *marks_texture;

		if (focus == child || parent_focused) {
			colors = &config->border_colors.focused;
			title_texture = child->title_focused;
			marks_texture = view ? view->marks_focused : NULL;
		} else if (child == current) {
			colors = &config->border_colors.focused_inactive;
			title_texture = child->title_focused_inactive;
			marks_texture = view ? view->marks_focused_inactive : NULL;
		} else {
			colors = &config->border_colors.unfocused;
			title_texture = child->title_unfocused;
			marks_texture = view ? view->marks_unfocused : NULL;
		}

		int y = pstate->swayc_y + container_titlebar_height() * i;
		render_titlebar(child, cstate->swayc_x, y, cstate->swayc_width, colors,
				title_texture, marks_texture);

		if (child == current) {
			current_colors = colors;
		}
	}

	// Render surface and left/right/bottom borders
	if (current) {
		if (current->type == C_VIEW) {
			render_view(current, current_colors);
		} else {
			render_container(current, parent_focused || current == focus);
		}
	}
}

static void render_container(struct sway_container *con, bool parent_focused) {
	switch (con->current.layout) {
	case L_NONE:
	case L_HORIZ:
	case L_VERT:
		render_container_simple(con, parent_focused);
		break;
	case L_STACKED:
		render_container_stacked(con, parent_focused);
		break;
	case L_TABBED:
		render_container_tabbed(con, parent_focused);
		break;
	case L_FLOATING:
		sway_assert(false, "Didn't expect to see floating here");
	}
}

static void render_floating_container(struct sway_container *con) {
	if (con->type == C_VIEW) {
		struct sway_view *view = con->sway_view;
		struct sway_seat *seat = input_manager_current_seat(input_manager);
		struct sway_container *focus = seat_get_focus(seat);
		struct border_colors *colors;
		struct wlr_texture *title_texture;
		struct wlr_texture *marks_texture;

		if (focus == con) {
			colors = &config->border_colors.focused;
			title_texture = con->title_focused;
			marks_texture = view->marks_focused;
		} else {
			colors = &config->border_colors.unfocused;
			title_texture = con->title_unfocused;
			marks_texture = view->marks_unfocused;
		}

		if (con->current.border == B_NORMAL) {
			render_titlebar(con, con->current.swayc_x, con->current.swayc_y,
					con->current.swayc_width, colors,
					title_texture, marks_texture);
		} else if (con->current.border != B_NONE) {
			render_top_border(con, colors);
		}
		render_view(con, colors);
	} else {
		render_container(con, false);
	}
}

static void render_floating() {
	for (int i = 0; i < root_container.current.children->length; ++i) {
		struct sway_container *output =
			root_container.current.children->items[i];
		for (int j = 0; j < output->current.children->length; ++j) {
			struct sway_container *ws = output->current.children->items[j];
			if (!workspace_is_visible(ws)) {
				continue;
			}
			list_t *floating =
				ws->current.ws_floating->current.children;
			for (int k = 0; k < floating->length; ++k) {
				struct sway_container *floater = floating->items[k];
				render_floating_container(floater);
			}
		}
	}
}

static struct sway_container *output_get_active_workspace(
		struct sway_output *output) {
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

void render_output(struct sway_output *output, struct timespec *when,
		pixman_region32_t *damage) {
	struct wlr_output *wlr_output = output->wlr_output;

	memset(&context, 0, sizeof(struct render_context));
	context.output = output;
	context.output_lx = output->swayc->current.swayc_x;
	context.output_ly = output->swayc->current.swayc_y;
	context.damage = damage;
	context.scale = output->wlr_output->scale;

	struct wlr_renderer *renderer =
	wlr_backend_get_renderer(wlr_output->backend);
	if (!sway_assert(renderer != NULL,
			"expected the output backend to have a renderer")) {
		return;
	}

	wlr_renderer_begin(renderer, wlr_output->width, wlr_output->height);

	bool damage_whole_before_swap = false;
	if (!pixman_region32_not_empty(damage)) {
		// Output isn't damaged but needs buffer swap
		goto renderer_end;
	}

	const char *damage_debug = getenv("SWAY_DAMAGE_DEBUG");
	if (damage_debug != NULL) {
		if (strcmp(damage_debug, "highlight") == 0) {
			wlr_renderer_clear(renderer, (float[]){1, 1, 0, 1});
			damage_whole_before_swap = true;
		} else if (strcmp(damage_debug, "rerender") == 0) {
			int width, height;
			wlr_output_transformed_resolution(wlr_output, &width, &height);
			pixman_region32_union_rect(damage, damage, 0, 0, width, height);
		}
	}

	struct sway_container *workspace = output_get_active_workspace(output);
	struct sway_view *fullscreen_view = workspace->current.ws_fullscreen;

	if (fullscreen_view) {
		float clear_color[] = {0.0f, 0.0f, 0.0f, 1.0f};

		int nrects;
		pixman_box32_t *rects = pixman_region32_rectangles(damage, &nrects);
		for (int i = 0; i < nrects; ++i) {
			scissor_output(wlr_output, &rects[i]);
			wlr_renderer_clear(renderer, clear_color);
		}

		// TODO: handle views smaller than the output
		render_surfaces(fullscreen_view->surface, 0, 0, 1.0f);

		if (fullscreen_view->type == SWAY_VIEW_XWAYLAND) {
			render_unmanaged(&root_container.sway_root->xwayland_unmanaged);
		}
	} else {
		float clear_color[] = {0.25f, 0.25f, 0.25f, 1.0f};

		int nrects;
		pixman_box32_t *rects = pixman_region32_rectangles(damage, &nrects);
		for (int i = 0; i < nrects; ++i) {
			scissor_output(wlr_output, &rects[i]);
			wlr_renderer_clear(renderer, clear_color);
		}

		render_layer(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
		render_layer(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

		struct sway_seat *seat = input_manager_current_seat(input_manager);
		struct sway_container *focus = seat_get_focus(seat);
		render_container(workspace, focus == workspace);
		render_floating();

		render_unmanaged(&root_container.sway_root->xwayland_unmanaged);
		render_layer(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
	}
	render_layer(&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);
	render_drag_icons(&root_container.sway_root->drag_icons);

renderer_end:
	if (root_container.sway_root->debug_tree) {
		wlr_render_texture(renderer, root_container.sway_root->debug_tree,
			wlr_output->transform_matrix, 0, 0, 1);
	}

	if (damage_whole_before_swap || root_container.sway_root->debug_tree) {
		int width, height;
		wlr_output_transformed_resolution(wlr_output, &width, &height);
		pixman_region32_union_rect(damage, damage, 0, 0, width, height);
	}

	wlr_renderer_scissor(renderer, NULL);
	wlr_renderer_end(renderer);
	if (!wlr_output_damage_swap_buffers(output->damage, when, damage)) {
		return;
	}
	output->last_frame = *when;
}
