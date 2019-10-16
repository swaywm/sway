#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/config.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/types/wlr_matrix.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/util/region.h>
#include "log.h"
#include "config.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

struct render_data {
	struct render_context *ctx;
	const pixman_region32_t *damage;
	float alpha;
	struct wlr_box *clip_box;
};

static void transform_output_damage(pixman_region32_t *damage, struct wlr_output *output) {
	int ow, oh;
	wlr_output_transformed_resolution(output, &ow, &oh);
	enum wl_output_transform transform =
		wlr_output_transform_invert(output->transform);
	wlr_region_transform(damage, damage, transform, ow, oh);
}

static void transform_output_box(struct wlr_box *box, struct wlr_output *output) {
	int ow, oh;
	wlr_output_transformed_resolution(output, &ow, &oh);
	enum wl_output_transform transform =
		wlr_output_transform_invert(output->transform);
	wlr_box_transform(box, box, transform, ow, oh);
}


/**
 * Apply scale to a width or height.
 *
 * One does not simply multiply the width by the scale. We allow fractional
 * scaling, which means the resulting scaled width might be a decimal.
 * So we round it.
 *
 * But even this can produce undesirable results depending on the X or Y offset
 * of the box. For example, with a scale of 1.5, a box with width=1 should not
 * scale to 2px if its X coordinate is 1, because the X coordinate would have
 * scaled to 2px.
 */
static int scale_length(int length, int offset, float scale) {
	return roundf((offset + length) * scale) - roundf(offset * scale);
}

static enum wlr_scale_filter_mode get_scale_filter(struct sway_output *output) {
	switch (output->scale_filter) {
	case SCALE_FILTER_LINEAR:
		return WLR_SCALE_FILTER_BILINEAR;
	case SCALE_FILTER_NEAREST:
		return WLR_SCALE_FILTER_NEAREST;
	default:
		abort(); // unreachable
	}
}

static void render_texture(struct render_context *ctx, struct wlr_texture *texture,
		const struct wlr_fbox *_src_box, const struct wlr_box *dst_box,
		const struct wlr_box *clip_box, enum wl_output_transform transform, float alpha) {
	struct sway_output *output = ctx->output;

	struct wlr_box proj_box = *dst_box;

	struct wlr_fbox src_box = {0};
	if (_src_box) {
		src_box = *_src_box;
	}

	pixman_region32_t damage;
	pixman_region32_init_rect(&damage, proj_box.x, proj_box.y,
		proj_box.width, proj_box.height);
	pixman_region32_intersect(&damage, &damage, ctx->output_damage);

	if (clip_box) {
		pixman_region32_intersect_rect(&damage, &damage,
				clip_box->x, clip_box->y, clip_box->width, clip_box->height);
	}

	bool damaged = pixman_region32_not_empty(&damage);
	if (!damaged) {
		goto damage_finish;
	}

	transform_output_box(&proj_box, output->wlr_output);
	transform_output_damage(&damage, output->wlr_output);
	transform = wlr_output_transform_compose(transform, output->wlr_output->transform);

	wlr_render_pass_add_texture(ctx->pass, &(struct wlr_render_texture_options) {
		.texture = texture,
		.src_box = src_box,
		.dst_box = proj_box,
		.transform = transform,
		.alpha = &alpha,
		.clip = &damage,
		.filter_mode = get_scale_filter(output),
	});

damage_finish:
	pixman_region32_fini(&damage);
}

static void render_surface_iterator(struct sway_output *output,
		struct sway_view *view, struct wlr_surface *surface,
		struct wlr_box *_box, void *_data) {
	struct render_data *data = _data;
	struct wlr_output *wlr_output = output->wlr_output;
	float alpha = data->alpha;

	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (!texture) {
		return;
	}

	struct wlr_fbox src_box;
	wlr_surface_get_buffer_source_box(surface, &src_box);

	struct wlr_box dst_box = *_box;
	struct wlr_box clip_box = *_box;
	if (data->clip_box != NULL) {
		clip_box.width = fmin(dst_box.width, data->clip_box->width);
		clip_box.height = fmin(dst_box.height, data->clip_box->height);
	}
	scale_box(&dst_box, wlr_output->scale);
	scale_box(&clip_box, wlr_output->scale);

	render_texture(data->ctx, texture,
		&src_box, &dst_box, &clip_box, surface->current.transform, alpha);

	wlr_presentation_surface_textured_on_output(server.presentation, surface,
		wlr_output);
}

static void render_layer_toplevel(struct render_context *ctx, struct wl_list *layer_surfaces) {
	struct render_data data = {
		.alpha = 1.0f,
		.ctx = ctx,
	};
	output_layer_for_each_toplevel_surface(ctx->output, layer_surfaces,
		render_surface_iterator, &data);
}

static void render_layer_popups(struct render_context *ctx, struct wl_list *layer_surfaces) {
	struct render_data data = {
		.alpha = 1.0f,
		.ctx = ctx,
	};
	output_layer_for_each_popup_surface(ctx->output, layer_surfaces,
		render_surface_iterator, &data);
}

#if HAVE_XWAYLAND
static void render_unmanaged(struct render_context *ctx, struct wl_list *unmanaged) {
	struct render_data data = {
		.alpha = 1.0f,
		.ctx = ctx,
	};
	output_unmanaged_for_each_surface(ctx->output, unmanaged,
		render_surface_iterator, &data);
}
#endif

static void render_drag_icons(struct render_context *ctx, struct wl_list *drag_icons) {
	struct render_data data = {
		.alpha = 1.0f,
		.ctx = ctx,
	};
	output_drag_icons_for_each_surface(ctx->output, drag_icons,
		render_surface_iterator, &data);
}

// _box.x and .y are expected to be layout-local
// _box.width and .height are expected to be output-buffer-local
void render_rect(struct render_context *ctx, const struct wlr_box *_box,
		float color[static 4]) {
	struct wlr_output *wlr_output = ctx->output->wlr_output;

	struct wlr_box box = *_box;
	box.x -= ctx->output->lx * wlr_output->scale;
	box.y -= ctx->output->ly * wlr_output->scale;

	pixman_region32_t damage;
	pixman_region32_init_rect(&damage, box.x, box.y,
		box.width, box.height);
	pixman_region32_intersect(&damage, &damage, ctx->output_damage);
	bool damaged = pixman_region32_not_empty(&damage);
	if (!damaged) {
		goto damage_finish;
	}

	transform_output_damage(&damage, wlr_output);
	transform_output_box(&box, wlr_output);

	wlr_render_pass_add_rect(ctx->pass, &(struct wlr_render_rect_options){
		.box = box,
		.color = {
			.r = color[0],
			.g = color[1],
			.b = color[2],
			.a = color[3],
		},
		.clip = &damage,
	});

damage_finish:
	pixman_region32_fini(&damage);
}

void premultiply_alpha(float color[4], float opacity) {
	color[3] *= opacity;
	color[0] *= color[3];
	color[1] *= color[3];
	color[2] *= color[3];
}

static void render_view_toplevels(struct render_context *ctx,
		struct sway_view *view, float alpha) {
	struct render_data data = {
		.alpha = alpha,
		.ctx = ctx,
	};
	struct wlr_box clip_box;
	if (!container_is_current_floating(view->container)) {
		// As we pass the geometry offsets to the surface iterator, we will
		// need to account for the offsets in the clip dimensions.
		clip_box.width = view->container->current.content_width + view->geometry.x;
		clip_box.height = view->container->current.content_height + view->geometry.y;
		data.clip_box = &clip_box;
	}
	// Render all toplevels without descending into popups
	double ox = view->container->surface_x -
		ctx->output->lx - view->geometry.x;
	double oy = view->container->surface_y -
		ctx->output->ly - view->geometry.y;
	output_surface_for_each_surface(ctx->output, view->surface, ox, oy,
			render_surface_iterator, &data);
}

static void render_view_popups(struct render_context *ctx, struct sway_view *view,
		float alpha) {
	struct render_data data = {
		.alpha = alpha,
		.ctx = ctx,
	};
	output_view_for_each_popup_surface(ctx->output, view,
		render_surface_iterator, &data);
}

static void render_saved_view(struct render_context *ctx, struct sway_view *view,
		float alpha) {
	struct sway_output *output = ctx->output;
	struct wlr_output *wlr_output = output->wlr_output;

	if (wl_list_empty(&view->saved_buffers)) {
		return;
	}

	bool floating = container_is_current_floating(view->container);

	struct sway_saved_buffer *saved_buf;
	wl_list_for_each(saved_buf, &view->saved_buffers, link) {
		if (!saved_buf->buffer->texture) {
			continue;
		}

		struct wlr_box proj_box = {
			.x = saved_buf->x - view->saved_geometry.x - output->lx,
			.y = saved_buf->y - view->saved_geometry.y - output->ly,
			.width = saved_buf->width,
			.height = saved_buf->height,
		};

		struct wlr_box output_box = {
			.width = output->width,
			.height = output->height,
		};

		struct wlr_box intersection;
		bool intersects = wlr_box_intersection(&intersection, &output_box, &proj_box);
		if (!intersects) {
			continue;
		}

		struct wlr_box dst_box = proj_box;
		struct wlr_box clip_box = proj_box;
		if (!floating) {
			clip_box.width = fmin(dst_box.width,
					view->container->current.content_width -
					(saved_buf->x - view->container->current.content_x) + view->saved_geometry.x);
			clip_box.height = fmin(dst_box.height,
					view->container->current.content_height -
					(saved_buf->y - view->container->current.content_y) + view->saved_geometry.y);
		}
		scale_box(&dst_box, wlr_output->scale);
		scale_box(&clip_box, wlr_output->scale);

		render_texture(ctx, saved_buf->buffer->texture,
			&saved_buf->source_box, &dst_box, &clip_box, saved_buf->transform, alpha);
	}

	// FIXME: we should set the surface that this saved buffer originates from
	// as sampled here.
	// https://github.com/swaywm/sway/pull/4465#discussion_r321082059
}

/**
 * Render a view's surface and left/bottom/right borders.
 */
static void render_view(struct render_context *ctx,
		struct sway_container *con, struct border_colors *colors) {
	struct sway_view *view = con->view;
	if (!wl_list_empty(&view->saved_buffers)) {
		render_saved_view(ctx, view, view->container->alpha);
	} else if (view->surface) {
		render_view_toplevels(ctx, view, view->container->alpha);
	}

	if (con->current.border == B_NONE || con->current.border == B_CSD) {
		return;
	}

	struct wlr_box box;
	float output_scale = ctx->output->wlr_output->scale;
	float color[4];
	struct sway_container_state *state = &con->current;

	if (state->border_left) {
		memcpy(&color, colors->child_border, sizeof(float) * 4);
		premultiply_alpha(color, con->alpha);
		box.x = floor(state->x);
		box.y = floor(state->content_y);
		box.width = state->border_thickness;
		box.height = state->content_height;
		scale_box(&box, output_scale);
		render_rect(ctx, &box, color);
	}

	list_t *siblings = container_get_current_siblings(con);
	enum sway_container_layout layout =
		container_current_parent_layout(con);

	if (state->border_right) {
		if (!container_is_current_floating(con) && siblings->length == 1 && layout == L_HORIZ) {
			memcpy(&color, colors->indicator, sizeof(float) * 4);
		} else {
			memcpy(&color, colors->child_border, sizeof(float) * 4);
		}
		premultiply_alpha(color, con->alpha);
		box.x = floor(state->content_x + state->content_width);
		box.y = floor(state->content_y);
		box.width = state->border_thickness;
		box.height = state->content_height;
		scale_box(&box, output_scale);
		render_rect(ctx, &box, color);
	}
}

/**
 * Render a titlebar.
 *
 * Care must be taken not to render over the same pixel multiple times,
 * otherwise the colors will be incorrect when using opacity.
 *
 * The height is: 1px border, 3px padding, font height, 3px padding, 1px border
 * The left side is: 1px border, 2px padding, title
 */
static void render_titlebar(struct render_context *ctx, struct sway_container *con,
		int x, int y, int width,
		struct border_colors *colors, struct wlr_texture *title_texture,
		struct wlr_texture *marks_texture) {
	struct wlr_box box;
	float color[4];
	struct sway_output *output = ctx->output;
	float output_scale = output->wlr_output->scale;
	double output_x = output->lx;
	double output_y = output->ly;
	int titlebar_border_thickness = config->titlebar_border_thickness;
	int titlebar_h_padding = config->titlebar_h_padding;
	int titlebar_v_padding = config->titlebar_v_padding;
	enum alignment title_align = config->title_align;

	// Single pixel bar above title
	memcpy(&color, colors->border, sizeof(float) * 4);
	premultiply_alpha(color, con->alpha);
	box.x = x;
	box.y = y;
	box.width = width;
	box.height = titlebar_border_thickness;
	scale_box(&box, output_scale);
	render_rect(ctx, &box, color);

	// Single pixel bar below title
	box.x = x;
	box.y = y + container_titlebar_height() - titlebar_border_thickness;
	box.width = width;
	box.height = titlebar_border_thickness;
	scale_box(&box, output_scale);
	render_rect(ctx, &box, color);

	// Single pixel left edge
	box.x = x;
	box.y = y + titlebar_border_thickness;
	box.width = titlebar_border_thickness;
	box.height = container_titlebar_height() - titlebar_border_thickness * 2;
	scale_box(&box, output_scale);
	render_rect(ctx, &box, color);

	// Single pixel right edge
	box.x = x + width - titlebar_border_thickness;
	box.y = y + titlebar_border_thickness;
	box.width = titlebar_border_thickness;
	box.height = container_titlebar_height() - titlebar_border_thickness * 2;
	scale_box(&box, output_scale);
	render_rect(ctx, &box, color);

	int inner_x = x - output_x + titlebar_h_padding;
	int bg_y = y + titlebar_border_thickness;
	size_t inner_width = width - titlebar_h_padding * 2;

	// output-buffer local
	int ob_inner_x = roundf(inner_x * output_scale);
	int ob_inner_width = scale_length(inner_width, inner_x, output_scale);
	int ob_bg_height = scale_length(
			(titlebar_v_padding - titlebar_border_thickness) * 2 +
			config->font_height, bg_y, output_scale);

	// Marks
	int ob_marks_x = 0; // output-buffer-local
	int ob_marks_width = 0; // output-buffer-local
	if (config->show_marks && marks_texture) {
		struct wlr_box texture_box = {
			.width = marks_texture->width,
			.height = marks_texture->height,
		};
		ob_marks_width = texture_box.width;

		// The marks texture might be shorter than the config->font_height, in
		// which case we need to pad it as evenly as possible above and below.
		int ob_padding_total = ob_bg_height - texture_box.height;
		int ob_padding_above = floor(ob_padding_total / 2.0);
		int ob_padding_below = ceil(ob_padding_total / 2.0);

		// Render texture. If the title is on the right, the marks will be on
		// the left. Otherwise, they will be on the right.
		if (title_align == ALIGN_RIGHT || texture_box.width > ob_inner_width) {
			texture_box.x = ob_inner_x;
		} else {
			texture_box.x = ob_inner_x + ob_inner_width - texture_box.width;
		}
		ob_marks_x = texture_box.x;

		texture_box.y = round((bg_y - output_y) * output_scale) +
			ob_padding_above;

		struct wlr_box clip_box = texture_box;
		if (ob_inner_width < clip_box.width) {
			clip_box.width = ob_inner_width;
		}
		render_texture(ctx, marks_texture,
			NULL, &texture_box, &clip_box, WL_OUTPUT_TRANSFORM_NORMAL, con->alpha);

		// Padding above
		memcpy(&color, colors->background, sizeof(float) * 4);
		premultiply_alpha(color, con->alpha);
		box.x = clip_box.x + round(output_x * output_scale);
		box.y = roundf((y + titlebar_border_thickness) * output_scale);
		box.width = clip_box.width;
		box.height = ob_padding_above;
		render_rect(ctx, &box, color);

		// Padding below
		box.y += ob_padding_above + clip_box.height;
		box.height = ob_padding_below;
		render_rect(ctx, &box, color);
	}

	// Title text
	int ob_title_x = 0;  // output-buffer-local
	int ob_title_width = 0; // output-buffer-local
	if (title_texture) {
		struct wlr_box texture_box = {
			.width = title_texture->width,
			.height = title_texture->height,
		};

		// The effective output may be NULL when con is not on any output.
		// This can happen because we render all children of containers,
		// even those that are out of the bounds of any output.
		struct sway_output *effective = container_get_effective_output(con);
		float title_scale = effective ? effective->wlr_output->scale : output_scale;
		texture_box.width = texture_box.width * output_scale / title_scale;
		texture_box.height = texture_box.height * output_scale / title_scale;
		ob_title_width = texture_box.width;

		// The title texture might be shorter than the config->font_height,
		// in which case we need to pad it above and below.
		int ob_padding_above = roundf((titlebar_v_padding -
					titlebar_border_thickness) * output_scale);
		int ob_padding_below = ob_bg_height - ob_padding_above -
			texture_box.height;

		// Render texture
		if (texture_box.width > ob_inner_width - ob_marks_width) {
			texture_box.x = (title_align == ALIGN_RIGHT && ob_marks_width)
				? ob_marks_x + ob_marks_width : ob_inner_x;
		} else if (title_align == ALIGN_LEFT) {
			texture_box.x = ob_inner_x;
		} else if (title_align == ALIGN_CENTER) {
			// If there are marks visible, center between the edge and marks.
			// Otherwise, center in the inner area.
			if (ob_marks_width) {
				texture_box.x = (ob_inner_x + ob_marks_x) / 2
					- texture_box.width / 2;
			} else {
				texture_box.x = ob_inner_x + ob_inner_width / 2
					- texture_box.width / 2;
			}
		} else {
			texture_box.x = ob_inner_x + ob_inner_width - texture_box.width;
		}
		ob_title_x = texture_box.x;

		texture_box.y =
			round((bg_y - output_y) * output_scale) + ob_padding_above;

		struct wlr_box clip_box = texture_box;
		if (ob_inner_width - ob_marks_width < clip_box.width) {
			clip_box.width = ob_inner_width - ob_marks_width;
		}

		render_texture(ctx, title_texture,
			NULL, &texture_box, &clip_box, WL_OUTPUT_TRANSFORM_NORMAL, con->alpha);

		// Padding above
		memcpy(&color, colors->background, sizeof(float) * 4);
		premultiply_alpha(color, con->alpha);
		box.x = clip_box.x + round(output_x * output_scale);
		box.y = roundf((y + titlebar_border_thickness) * output_scale);
		box.width = clip_box.width;
		box.height = ob_padding_above;
		render_rect(ctx, &box, color);

		// Padding below
		box.y += ob_padding_above + clip_box.height;
		box.height = ob_padding_below;
		render_rect(ctx, &box, color);
	}

	// Determine the left + right extends of the textures (output-buffer local)
	int ob_left_x, ob_left_width, ob_right_x, ob_right_width;
	if (ob_title_width == 0 && ob_marks_width == 0) {
		ob_left_x = ob_inner_x;
		ob_left_width = 0;
		ob_right_x = ob_inner_x;
		ob_right_width = 0;
	} else if (ob_title_x < ob_marks_x) {
		ob_left_x = ob_title_x;
		ob_left_width = ob_title_width;
		ob_right_x = ob_marks_x;
		ob_right_width = ob_marks_width;
	} else {
		ob_left_x = ob_marks_x;
		ob_left_width = ob_marks_width;
		ob_right_x = ob_title_x;
		ob_right_width = ob_title_width;
	}
	if (ob_left_x < ob_inner_x) {
		ob_left_x = ob_inner_x;
	} else if (ob_left_x + ob_left_width > ob_right_x + ob_right_width) {
		ob_right_x = ob_left_x;
		ob_right_width = ob_left_width;
	}

	// Filler between title and marks
	box.width = ob_right_x - ob_left_x - ob_left_width;
	if (box.width > 0) {
		box.x = ob_left_x + ob_left_width + round(output_x * output_scale);
		box.y = roundf(bg_y * output_scale);
		box.height = ob_bg_height;
		render_rect(ctx, &box, color);
	}

	// Padding on left side
	box.x = x + titlebar_border_thickness;
	box.y = y + titlebar_border_thickness;
	box.width = titlebar_h_padding - titlebar_border_thickness;
	box.height = (titlebar_v_padding - titlebar_border_thickness) * 2 +
		config->font_height;
	scale_box(&box, output_scale);
	int left_x = ob_left_x + round(output_x * output_scale);
	if (box.x + box.width < left_x) {
		box.width += left_x - box.x - box.width;
	}
	render_rect(ctx, &box, color);

	// Padding on right side
	box.x = x + width - titlebar_h_padding;
	box.y = y + titlebar_border_thickness;
	box.width = titlebar_h_padding - titlebar_border_thickness;
	box.height = (titlebar_v_padding - titlebar_border_thickness) * 2 +
		config->font_height;
	scale_box(&box, output_scale);
	int right_rx = ob_right_x + ob_right_width + round(output_x * output_scale);
	if (right_rx < box.x) {
		box.width += box.x - right_rx;
		box.x = right_rx;
	}
	render_rect(ctx, &box, color);
}

/**
 * Render the top border line for a view using "border pixel".
 */
static void render_top_border(struct render_context *ctx, struct sway_container *con,
		struct border_colors *colors) {
	struct sway_container_state *state = &con->current;
	if (!state->border_top) {
		return;
	}
	struct wlr_box box;
	float color[4];
	float output_scale = ctx->output->wlr_output->scale;

	// Child border - top edge
	memcpy(&color, colors->child_border, sizeof(float) * 4);
	premultiply_alpha(color, con->alpha);
	box.x = floor(state->x);
	box.y = floor(state->y);
	box.width = state->width;
	box.height = state->border_thickness;
	scale_box(&box, output_scale);
	render_rect(ctx, &box, color);
}

/**
 * Render the bottom border line for a view using "border pixel".
 */
static void render_bottom_border(struct render_context *ctx, struct sway_container *con,
		struct border_colors *colors) {
	struct sway_container_state *state = &con->current;
	if (!state->border_bottom) {
		return;
	}
	struct wlr_box box;
	float color[4];
	float output_scale = ctx->output->wlr_output->scale;

	list_t *siblings = container_get_current_siblings(con);
	enum sway_container_layout layout =
		container_current_parent_layout(con);

	if (!container_is_current_floating(con) && siblings->length == 1 && layout == L_VERT) {
		memcpy(&color, colors->indicator, sizeof(float) * 4);
	} else {
		memcpy(&color, colors->child_border, sizeof(float) * 4);
	}
	premultiply_alpha(color, con->alpha);
	box.x = floor(state->x);
	box.y = floor(state->content_y + state->content_height);
	box.width = state->width;
	box.height = state->border_thickness;
	scale_box(&box, output_scale);
	render_rect(ctx, &box, color);
}

struct parent_data {
	enum sway_container_layout layout;
	struct wlr_box box;
	list_t *children;
	bool focused;
	struct sway_container *active_child;
};

static void render_container(struct render_context *ctx,
	struct sway_container *con, bool parent_focused);

/**
 * Render a container's children using a L_HORIZ or L_VERT layout.
 *
 * Wrap child views in borders and leave child containers borderless because
 * they'll apply their own borders to their children.
 */
static void render_containers_linear(struct render_context *ctx, struct parent_data *parent) {
	for (int i = 0; i < parent->children->length; ++i) {
		struct sway_container *child = parent->children->items[i];

		if (child->view) {
			struct sway_view *view = child->view;
			struct border_colors *colors;
			struct wlr_texture *title_texture;
			struct wlr_texture *marks_texture;
			struct sway_container_state *state = &child->current;

			if (view_is_urgent(view)) {
				colors = &config->border_colors.urgent;
				title_texture = child->title_urgent;
				marks_texture = child->marks_urgent;
			} else if (state->focused || parent->focused) {
				colors = &config->border_colors.focused;
				title_texture = child->title_focused;
				marks_texture = child->marks_focused;
			} else if (child == parent->active_child) {
				colors = &config->border_colors.focused_inactive;
				title_texture = child->title_focused_inactive;
				marks_texture = child->marks_focused_inactive;
			} else {
				colors = &config->border_colors.unfocused;
				title_texture = child->title_unfocused;
				marks_texture = child->marks_unfocused;
			}

			if (state->border == B_NORMAL) {
				render_titlebar(ctx, child, floor(state->x),
						floor(state->y), state->width, colors,
						title_texture, marks_texture);
			} else if (state->border == B_PIXEL) {
				render_top_border(ctx, child, colors);
			}
			render_view(ctx, child, colors);
			if (config->titlebar_position != TITLEBAR_BOTTOM) {
				render_bottom_border(ctx, child, colors);
			} else {
				render_top_border(ctx, child, colors);
			}
		} else {
			render_container(ctx, child,
					parent->focused || child->current.focused);
		}
	}
}

static bool container_is_focused(struct sway_container *con, void *data) {
	return con->current.focused;
}

static bool container_has_focused_child(struct sway_container *con) {
	return container_find_child(con, container_is_focused, NULL);
}

/**
 * Render a container's children using the L_TABBED layout.
 */
static void render_containers_tabbed(struct render_context *ctx, struct parent_data *parent) {
	if (!parent->children->length) {
		return;
	}
	struct sway_container *current = parent->active_child;
	struct border_colors *current_colors = &config->border_colors.unfocused;
	int tab_width = parent->box.width / parent->children->length;

	// Render tabs
	for (int i = 0; i < parent->children->length; ++i) {
		struct sway_container *child = parent->children->items[i];
		struct sway_view *view = child->view;
		struct sway_container_state *cstate = &child->current;
		struct border_colors *colors;
		struct wlr_texture *title_texture;
		struct wlr_texture *marks_texture;
		bool urgent = view ?
			view_is_urgent(view) : container_has_urgent_child(child);

		if (urgent) {
			colors = &config->border_colors.urgent;
			title_texture = child->title_urgent;
			marks_texture = child->marks_urgent;
		} else if (cstate->focused || parent->focused) {
			colors = &config->border_colors.focused;
			title_texture = child->title_focused;
			marks_texture = child->marks_focused;
		} else if (config->has_focused_tab_title && container_has_focused_child(child)) {
			colors = &config->border_colors.focused_tab_title;
			title_texture = child->title_focused_tab_title;
			marks_texture = child->marks_focused_tab_title;
		} else if (child == parent->active_child) {
			colors = &config->border_colors.focused_inactive;
			title_texture = child->title_focused_inactive;
			marks_texture = child->marks_focused_inactive;
		} else {
			colors = &config->border_colors.unfocused;
			title_texture = child->title_unfocused;
			marks_texture = child->marks_unfocused;
		}

		int x = floor(cstate->x + tab_width * i);

		// Make last tab use the remaining width of the parent
		if (i == parent->children->length - 1) {
			tab_width = parent->box.width - tab_width * i;
		}

		render_titlebar(ctx, child, x, parent->box.y, tab_width,
				colors, title_texture, marks_texture);

		if (child == current) {
			current_colors = colors;
		}
	}

	// Render surface and left/right/bottom borders
	if (current->view) {
		render_view(ctx, current, current_colors);
		if (config->titlebar_position != TITLEBAR_BOTTOM) {
			render_bottom_border(ctx, current, current_colors);
		} else {
			render_top_border(ctx, current, current_colors);
		}
	} else {
		render_container(ctx, current,
				parent->focused || current->current.focused);
	}
}

/**
 * Render a container's children using the L_STACKED layout.
 */
static void render_containers_stacked(struct render_context *ctx, struct parent_data *parent) {
	if (!parent->children->length) {
		return;
	}
	struct sway_container *current = parent->active_child;
	struct border_colors *current_colors = &config->border_colors.unfocused;
	size_t titlebar_height = container_titlebar_height();

	// Render titles
	for (int i = 0; i < parent->children->length; ++i) {
		struct sway_container *child = parent->children->items[i];
		struct sway_view *view = child->view;
		struct sway_container_state *cstate = &child->current;
		struct border_colors *colors;
		struct wlr_texture *title_texture;
		struct wlr_texture *marks_texture;
		bool urgent = view ?
			view_is_urgent(view) : container_has_urgent_child(child);

		if (urgent) {
			colors = &config->border_colors.urgent;
			title_texture = child->title_urgent;
			marks_texture = child->marks_urgent;
		} else if (cstate->focused || parent->focused) {
			colors = &config->border_colors.focused;
			title_texture = child->title_focused;
			marks_texture = child->marks_focused;
		} else if (config->has_focused_tab_title && container_has_focused_child(child)) {
			colors = &config->border_colors.focused_tab_title;
			title_texture = child->title_focused_tab_title;
			marks_texture = child->marks_focused_tab_title;
		 } else if (child == parent->active_child) {
			colors = &config->border_colors.focused_inactive;
			title_texture = child->title_focused_inactive;
			marks_texture = child->marks_focused_inactive;
		} else {
			colors = &config->border_colors.unfocused;
			title_texture = child->title_unfocused;
			marks_texture = child->marks_unfocused;
		}

		int y = parent->box.y + titlebar_height * i;
		render_titlebar(ctx, child, parent->box.x, y,
				parent->box.width, colors, title_texture, marks_texture);

		if (child == current) {
			current_colors = colors;
		}
	}

	// Render surface and left/right/bottom borders
	if (current->view) {
		if (titlebar_is_on_top) {
			render_bottom_border(ctx, current, current_colors);
		} else {
			render_top_border(ctx, current, current_colors);
		}
		render_view(ctx, current, current_colors);
	} else {
		render_container(ctx, current,
				parent->focused || current->current.focused);
	}
}

static void render_containers(struct render_context *ctx, struct parent_data *parent) {
	if (config->hide_lone_tab && parent->children->length == 1) {
		struct sway_container *child = parent->children->items[0];
		if (child->view) {
			render_containers_linear(ctx, parent);
			return;
		}
	}

	switch (parent->layout) {
	case L_NONE:
	case L_HORIZ:
	case L_VERT:
		render_containers_linear(ctx, parent);
		break;
	case L_STACKED:
		render_containers_stacked(ctx, parent);
		break;
	case L_TABBED:
		render_containers_tabbed(ctx, parent);
		break;
	}
}

static void render_container(struct render_context *ctx,
		struct sway_container *con, bool focused) {
	struct parent_data data = {
		.layout = con->current.layout,
		.box = {
			.x = floor(con->current.x),
			.y = floor(con->current.y),
			.width = con->current.width,
			.height = con->current.height,
		},
		.children = con->current.children,
		.focused = focused,
		.active_child = con->current.focused_inactive_child,
	};
	render_containers(ctx, &data);
}

static void render_workspace(struct render_context *ctx,
		struct sway_workspace *ws, bool focused) {
	struct parent_data data = {
		.layout = ws->current.layout,
		.box = {
			.x = floor(ws->current.x),
			.y = floor(ws->current.y),
			.width = ws->current.width,
			.height = ws->current.height,
		},
		.children = ws->current.tiling,
		.focused = focused,
		.active_child = ws->current.focused_inactive_child,
	};
	render_containers(ctx, &data);
}

static void render_floating_container(struct render_context *ctx,
		struct sway_container *con) {
	if (con->view) {
		struct sway_view *view = con->view;
		struct border_colors *colors;
		struct wlr_texture *title_texture;
		struct wlr_texture *marks_texture;

		if (view_is_urgent(view)) {
			colors = &config->border_colors.urgent;
			title_texture = con->title_urgent;
			marks_texture = con->marks_urgent;
		} else if (con->current.focused) {
			colors = &config->border_colors.focused;
			title_texture = con->title_focused;
			marks_texture = con->marks_focused;
		} else {
			colors = &config->border_colors.unfocused;
			title_texture = con->title_unfocused;
			marks_texture = con->marks_unfocused;
		}

		if (con->current.border == B_NORMAL) {
			render_titlebar(ctx, con, floor(con->current.x),
					floor(con->current.y), con->current.width, colors,
					title_texture, marks_texture);
		} else if (con->current.border == B_PIXEL) {
			render_top_border(ctx, con, colors);
		}
		if (config->titlebar_position != TITLEBAR_BOTTOM) {
			render_bottom_border(ctx, con, colors);
		} else {
			render_top_border(ctx, con, colors);
		}
		render_view(ctx, con, colors);
	} else {
		render_container(ctx, con, con->current.focused);
	}
}

static void render_floating(struct render_context *ctx) {
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		for (int j = 0; j < output->current.workspaces->length; ++j) {
			struct sway_workspace *ws = output->current.workspaces->items[j];
			if (!workspace_is_visible(ws)) {
				continue;
			}
			for (int k = 0; k < ws->current.floating->length; ++k) {
				struct sway_container *floater = ws->current.floating->items[k];
				if (floater->current.fullscreen_mode != FULLSCREEN_NONE) {
					continue;
				}
				render_floating_container(ctx, floater);
			}
		}
	}
}

static void render_seatops(struct render_context *ctx) {
	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		seatop_render(seat, ctx);
	}
}

void output_render(struct render_context *ctx) {
	struct wlr_output *wlr_output = ctx->output->wlr_output;
	struct sway_output *output = ctx->output;
	const pixman_region32_t *damage = ctx->output_damage;

	struct sway_workspace *workspace = output->current.active_workspace;
	if (workspace == NULL) {
		return;
	}

	struct sway_container *fullscreen_con = root->fullscreen_global;
	if (!fullscreen_con) {
		fullscreen_con = workspace->current.fullscreen;
	}

	if (!pixman_region32_not_empty(damage)) {
		// Output isn't damaged but needs buffer swap
		return;
	}

	if (debug.damage == DAMAGE_HIGHLIGHT) {
		wlr_render_pass_add_rect(ctx->pass, &(struct wlr_render_rect_options){
			.box = { .width = wlr_output->width, .height = wlr_output->height },
			.color = { .r = 1, .g = 1, .b = 0, .a = 1 },
		});
	}

	pixman_region32_t transformed_damage;
	pixman_region32_init(&transformed_damage);
	pixman_region32_copy(&transformed_damage, damage);
	transform_output_damage(&transformed_damage, wlr_output);

	if (server.session_lock.locked) {
		struct wlr_render_color clear_color = {
			.a = 1.0f
		};
		if (server.session_lock.lock == NULL) {
			// abandoned lock -> red BG
			clear_color.r = 1.f;
		}

		wlr_render_pass_add_rect(ctx->pass, &(struct wlr_render_rect_options){
			.box = { .width = wlr_output->width, .height = wlr_output->height },
			.color = clear_color,
			.clip = &transformed_damage,
		});

		if (server.session_lock.lock != NULL) {
			struct render_data data = {
				.alpha = 1.0f,
				.ctx = ctx,
			};

			struct wlr_session_lock_surface_v1 *lock_surface;
			wl_list_for_each(lock_surface, &server.session_lock.lock->surfaces, link) {
				if (lock_surface->output != wlr_output) {
					continue;
				}
				if (!lock_surface->surface->mapped) {
					continue;
				}

				output_surface_for_each_surface(output, lock_surface->surface,
					0.0, 0.0, render_surface_iterator, &data);
			}
		}
		goto renderer_end;
	}

	if (output_has_opaque_overlay_layer_surface(output)) {
		goto render_overlay;
	}

	if (fullscreen_con) {
		wlr_render_pass_add_rect(ctx->pass, &(struct wlr_render_rect_options){
			.box = { .width = wlr_output->width, .height = wlr_output->height },
			.color = { .r = 0, .g = 0, .b = 0, .a = 1 },
			.clip = &transformed_damage,
		});

		if (fullscreen_con->view) {
			if (!wl_list_empty(&fullscreen_con->view->saved_buffers)) {
				render_saved_view(ctx, fullscreen_con->view, 1.0f);
			} else if (fullscreen_con->view->surface) {
				render_view_toplevels(ctx, fullscreen_con->view, 1.0f);
			}
		} else {
			render_container(ctx, fullscreen_con,
					fullscreen_con->current.focused);
		}

		for (int i = 0; i < workspace->current.floating->length; ++i) {
			struct sway_container *floater =
				workspace->current.floating->items[i];
			if (container_is_transient_for(floater, fullscreen_con)) {
				render_floating_container(ctx, floater);
			}
		}
#if HAVE_XWAYLAND
		render_unmanaged(ctx, &root->xwayland_unmanaged);
#endif
	} else {
		wlr_render_pass_add_rect(ctx->pass, &(struct wlr_render_rect_options){
			.box = { .width = wlr_output->width, .height = wlr_output->height },
			.color = { .r = 0.25f, .g = 0.25f, .b = 0.25f, .a = 1 },
			.clip = &transformed_damage,
		});

		render_layer_toplevel(ctx,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
		render_layer_toplevel(ctx,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

		render_workspace(ctx, workspace, workspace->current.focused);
		render_floating(ctx);
#if HAVE_XWAYLAND
		render_unmanaged(ctx, &root->xwayland_unmanaged);
#endif
		render_layer_toplevel(ctx,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);

		render_layer_popups(ctx,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
		render_layer_popups(ctx,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);
		render_layer_popups(ctx,
			&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
	}

	render_seatops(ctx);

	struct sway_seat *seat = input_manager_current_seat();
	struct sway_container *focus = seat_get_focused_container(seat);
	if (focus && focus->view) {
		render_view_popups(ctx, focus->view, focus->alpha);
	}

render_overlay:
	render_layer_toplevel(ctx,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);
	render_layer_popups(ctx,
		&output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);
	render_drag_icons(ctx, &root->drag_icons);

renderer_end:
	pixman_region32_fini(&transformed_damage);
	wlr_output_add_software_cursors_to_render_pass(wlr_output, ctx->pass, damage);
}
