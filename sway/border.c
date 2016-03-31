#include "border.h"
#include <wlc/wlc-render.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <stdio.h>
#include "container.h"
#include "config.h"
#include "client/pango.h"

#include <arpa/inet.h>

void cairo_set_source_u32(cairo_t *cairo, uint32_t color) {
    color = htonl(color);

    cairo_set_source_rgba(cairo,
            (color >> (2*8) & 0xFF) / 255.0,
            (color >> (1*8) & 0xFF) / 255.0,
            (color >> (0*8) & 0xFF) / 255.0,
            (color >> (3*8) & 0xFF) / 255.0);
}

static cairo_t *create_border_buffer(swayc_t *view, struct wlc_geometry geo, cairo_surface_t **surface) {
	cairo_t *cr;
	view->border_geometry = geo;
	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, geo.size.w);
	view->border = calloc(stride * geo.size.h, sizeof(unsigned char));
	if (!view->border) {
		sway_log(L_DEBUG, "Unable to allocate buffer");
		return NULL;
	}
	*surface = cairo_image_surface_create_for_data(view->border,
			CAIRO_FORMAT_ARGB32, geo.size.w, geo.size.h, stride);
	if (cairo_surface_status(*surface) != CAIRO_STATUS_SUCCESS) {
		free(view->border);
		view->border = NULL;
		sway_log(L_DEBUG, "Unable to allocate surface");
		return NULL;
	}
	cr = cairo_create(*surface);
	if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(*surface);
		free(view->border);
		view->border = NULL;
		sway_log(L_DEBUG, "Unable to create cairo context");
		return NULL;
	}
	return cr;
}

// TODO: move to client/cairo.h when local set_source_u32 is fixed.
/**
 * Renders a sharp line of any width and height.
 *
 * The line is drawn from (x,y) to (x+width,y+height) where width/height is 0
 * if the line has a width/height of one pixel, respectively.
 */
static void render_sharp_line(cairo_t *cairo, uint32_t color, double x, double y, double width, double height) {
	cairo_set_source_u32(cairo, color);

	if (width > 1 && height > 1) {
		cairo_rectangle(cairo, x, y, width, height);
		cairo_fill(cairo);
	} else {
		if (width == 1) {
			x += 0.5;
			height += y;
			width = x;
		}

		if (height == 1) {
			y += 0.5;
			width += x;
			height = y;
		}

		cairo_move_to(cairo, x, y);
		cairo_set_line_width(cairo, 1.0);
		cairo_line_to(cairo, width, height);
		cairo_stroke(cairo);
	}
}

int get_font_text_height(const char *font) {
	cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 200, 200);
	cairo_t *cr = cairo_create(surface);
	int width, height;
	get_text_size(cr, font, &width, &height, "Gg");
	return height;
}

static void render_borders(swayc_t *view, cairo_t *cr, struct border_colors *colors) {
	struct wlc_geometry *b = &view->border_geometry;
	struct wlc_geometry *v = &view->actual_geometry;

	// left border
	int left_border = v->origin.x - b->origin.x;
	if (left_border > 0) {
		render_sharp_line(cr,
				colors->child_border,
				0, 0,
				left_border,
				b->size.h);
	}

	// right border
	int right_border = b->size.w - v->size.w - left_border;
	if (right_border > 0) {
		render_sharp_line(cr,
				colors->child_border,
				b->size.w - right_border,
				0,
				right_border,
				b->size.h);
	}

	// top border
	int top_border = v->origin.y - b->origin.y;
	if (top_border > 0) {
		render_sharp_line(cr,
				colors->child_border,
				0, 0,
				b->size.w,
				top_border);
	}

	// bottom border
	int bottom_border = b->size.h - (top_border + v->size.h);
	if (bottom_border > 0) {
		render_sharp_line(cr,
				colors->child_border,
				0,
				b->size.h - bottom_border,
				b->size.w,
				bottom_border);
	}
}

static void render_with_title_bar(swayc_t *view, cairo_t *cr, struct border_colors *colors) {
	struct wlc_geometry *tb = &view->title_bar_geometry;
	struct wlc_geometry *b = &view->border_geometry;

	// borders
	render_borders(view, cr, colors);

	// title bar background
	cairo_set_source_u32(cr, colors->child_border);
	cairo_rectangle(cr, 0, 0, tb->size.w, tb->size.h);
	cairo_fill(cr);

	// header top line
	render_sharp_line(cr, colors->border, 0, 0, tb->size.w, 1);

	// text
	if (view->name) {
		int width, height;
		get_text_size(cr, config->font, &width, &height, "%s", view->name);
		cairo_move_to(cr, view->border_thickness, 2);
		cairo_set_source_u32(cr, colors->text);
		pango_printf(cr, config->font, "%s", view->name);
	}

	// header bottom line
	render_sharp_line(cr, colors->border,
			view->actual_geometry.origin.x - b->origin.x,
			tb->size.h - 1,
			view->actual_geometry.size.w, 1);
}

void map_update_view_border(swayc_t *view, void *data) {
	if (view->type == C_VIEW) {
		update_view_border(view);
	}
}

void update_view_border(swayc_t *view) {
	cairo_t *cr = NULL;
	cairo_surface_t *surface = NULL;

	if (view->border) {
		free(view->border);
		view->border = NULL;
	}

	swayc_t *focused = get_focused_view(&root_container);
	swayc_t *container = swayc_parent_by_type(view, C_CONTAINER);
	swayc_t *focused_inactive = NULL;
	if (container) {
		focused_inactive = swayc_focus_by_type(container, C_VIEW);
	} else {
		container = swayc_parent_by_type(view, C_WORKSPACE);
		if (container) {
			focused_inactive = swayc_focus_by_type(container, C_VIEW);
		}
	}

	switch (view->border_type) {
	case B_NONE:
		break;
	case B_PIXEL:
		cr = create_border_buffer(view, view->border_geometry, &surface);
		if (!cr) {
			break;
		}

		if (focused == view) {
			render_borders(view, cr, &config->border_colors.focused);
		} else if (focused_inactive == view) {
			render_borders(view, cr, &config->border_colors.focused_inactive);
		} else {
			render_borders(view, cr, &config->border_colors.unfocused);
		}

		break;
	case B_NORMAL:
		cr = create_border_buffer(view, view->border_geometry, &surface);
		if (!cr) {
			break;
		}

		if (focused == view) {
			render_with_title_bar(view, cr, &config->border_colors.focused);
		} else if (focused_inactive == view) {
			render_with_title_bar(view, cr, &config->border_colors.focused_inactive);
		} else {
			render_with_title_bar(view, cr, &config->border_colors.unfocused);
		}

		break;
	}
	if (surface) {
		cairo_surface_flush(surface);
		cairo_surface_destroy(surface);
	}
	if (cr) {
		cairo_destroy(cr);
	}
}

void render_view_borders(wlc_handle view) {
	swayc_t *c = swayc_by_handle(view);

	if (!c || c->border_type == B_NONE) {
		return;
	}

	if (c->border_type == B_NORMAL) {
		// update window title
		const char *new_name = wlc_view_get_title(view);

		if (new_name) {
			if (!c->name || strcmp(c->name, new_name) != 0) {
				free(c->name);
				c->name = strdup(new_name);
				update_view_border(c);
			}
		}
	}

	if (c->border) {
		wlc_pixels_write(WLC_RGBA8888, &c->border_geometry, c->border);
	}
}
