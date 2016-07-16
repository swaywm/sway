#include "border.h"
#include <wlc/wlc-render.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "container.h"
#include "config.h"
#include "client/pango.h"

void cairo_set_source_u32(cairo_t *cairo, uint32_t color) {
	color = htonl(color);

	cairo_set_source_rgba(cairo,
		(color >> (2*8) & 0xFF) / 255.0,
		(color >> (1*8) & 0xFF) / 255.0,
		(color >> (0*8) & 0xFF) / 255.0,
		(color >> (3*8) & 0xFF) / 255.0);
}

void border_clear(struct border *border) {
	if (border && border->buffer) {
		free(border->buffer);
		border->buffer = NULL;
	}
}

static cairo_t *create_border_buffer(swayc_t *view, struct wlc_geometry g, cairo_surface_t **surface) {
	if (view->border == NULL) {
		view->border = malloc(sizeof(struct border));
	}
	cairo_t *cr;
	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, g.size.w);
	view->border->buffer = calloc(stride * g.size.h, sizeof(unsigned char));
	view->border->geometry = g;
	if (!view->border->buffer) {
		sway_log(L_DEBUG, "Unable to allocate buffer");
		return NULL;
	}
	*surface = cairo_image_surface_create_for_data(view->border->buffer,
			CAIRO_FORMAT_ARGB32, g.size.w, g.size.h, stride);
	if (cairo_surface_status(*surface) != CAIRO_STATUS_SUCCESS) {
		border_clear(view->border);
		sway_log(L_DEBUG, "Unable to allocate surface");
		return NULL;
	}
	cr = cairo_create(*surface);
	if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
		cairo_surface_destroy(*surface);
		border_clear(view->border);
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
	get_text_size(cr, font, &width, &height, false, "Gg");
	cairo_surface_destroy(surface);
	cairo_destroy(cr);
	return height;
}

static void render_borders(swayc_t *view, cairo_t *cr, struct border_colors *colors, bool top) {
	struct wlc_geometry *g = &view->border->geometry;
	struct wlc_geometry *b = &view->border_geometry;
	struct wlc_geometry *v = &view->actual_geometry;

	int x = b->origin.x - g->origin.x;
	int y = b->origin.y - g->origin.y;

	// left border
	int left_border = v->origin.x - b->origin.x;
	if (left_border > 0) {
		render_sharp_line(cr,
				colors->child_border,
				x, y,
				left_border,
				b->size.h);
	}

	// right border
	int right_border = b->size.w - v->size.w - left_border;
	if (right_border > 0) {
		render_sharp_line(cr,
				colors->child_border,
				x + b->size.w - right_border,
				y,
				right_border,
				b->size.h);
	}

	// top border
	int top_border = v->origin.y - b->origin.y;
	if (top && top_border > 0) {
		render_sharp_line(cr,
				colors->child_border,
				x, y,
				b->size.w,
				top_border);
	}

	// bottom border
	int bottom_border = b->size.h - (top_border + v->size.h);
	if (bottom_border > 0) {
		render_sharp_line(cr,
				colors->child_border,
				x,
				y + b->size.h - bottom_border,
				b->size.w,
				bottom_border);
	}
}

static void render_title_bar(swayc_t *view, cairo_t *cr, struct wlc_geometry *b, struct border_colors *colors) {
	struct wlc_geometry *tb = &view->title_bar_geometry;
	int x = MIN(tb->origin.x, tb->origin.x - b->origin.x);
	int y = MIN(tb->origin.y, tb->origin.y - b->origin.y);

	// title bar background
	cairo_set_source_u32(cr, colors->background);
	cairo_rectangle(cr, x, y, tb->size.w, tb->size.h);
	cairo_fill(cr);

	// header top line
	render_sharp_line(cr, colors->border, x, y, tb->size.w, 1);

	// text
	if (view->name) {
		int width, height;
		get_text_size(cr, config->font, &width, &height, false, "%s", view->name);
		cairo_move_to(cr, x + 2, y + 2);
		cairo_set_source_u32(cr, colors->text);
		pango_printf(cr, config->font, false, "%s", view->name);
	}

	// titlebars has a border all around for tabbed layouts
	if (view->parent->layout == L_TABBED) {
		// header bottom line
		render_sharp_line(cr, colors->border, x, y + tb->size.h - 1,
				tb->size.w, 1);

		// left border
		render_sharp_line(cr, colors->border, x, y, 1, tb->size.h);

		// right border
		render_sharp_line(cr, colors->border, x + tb->size.w - 1, y,
				1, tb->size.h);

		return;
	}

	if ((uint32_t)(view->actual_geometry.origin.y - tb->origin.y) == tb->size.h) {
		// header bottom line
		render_sharp_line(cr, colors->border,
				x + view->actual_geometry.origin.x - tb->origin.x,
				y + tb->size.h - 1,
				view->actual_geometry.size.w, 1);
	} else {
		// header bottom line
		render_sharp_line(cr, colors->border, x,
				y + tb->size.h - 1,
				tb->size.w, 1);
	}
}

void map_update_view_border(swayc_t *view, void *data) {
	if (view->type == C_VIEW) {
		update_view_border(view);
	}
}

/**
 * Generate nested container title for tabbed/stacked layouts
 */
static char *generate_container_title(swayc_t *container) {
	char layout = 'H';
	char *name, *prev_name = NULL;
	switch (container->layout) {
	case L_TABBED:
		layout = 'T';
		break;
	case L_STACKED:
		layout = 'S';
		break;
	case L_VERT:
		layout = 'V';
		break;
	default:
		layout = 'H';
	}
	int len = 9;
	name = malloc(len * sizeof(char));
	snprintf(name, len, "sway: %c[", layout);

	int i;
	for (i = 0; i < container->children->length; ++i) {
		prev_name = name;
		swayc_t* child = container->children->items[i];
		const char *title = child->name;
		if (child->type == C_CONTAINER) {
			title = generate_container_title(child);
		}

		len = strlen(name) + strlen(title) + 1;
		if (i < container->children->length-1) {
			len++;
		}

		name = malloc(len * sizeof(char));
		if (i < container->children->length-1) {
			snprintf(name, len, "%s%s ", prev_name, title);
		} else {
			snprintf(name, len, "%s%s", prev_name, title);
		}
		free(prev_name);
	}

	prev_name = name;
	len = strlen(name) + 2;
	name = malloc(len * sizeof(char));
	snprintf(name, len, "%s]", prev_name);
	free(prev_name);
	free(container->name);
	container->name = name;
	return container->name + 6; // don't include "sway: "
}

void update_tabbed_stacked_titlebars(swayc_t *c, cairo_t *cr, struct wlc_geometry *g, swayc_t *focused, swayc_t *focused_inactive) {
	if (c->type == C_CONTAINER) {
		if (c->parent->focused == c) {
			render_title_bar(c, cr, g, &config->border_colors.focused_inactive);
		} else {
			render_title_bar(c, cr, g, &config->border_colors.unfocused);
		}

		if (!c->visible) {
			return;
		}

		int i;
		for (i = 0; i < c->children->length; ++i) {
			swayc_t *child = c->children->items[i];
			update_tabbed_stacked_titlebars(child, cr, g, focused, focused_inactive);
		}
	} else {
		if (focused == c) {
			render_title_bar(c, cr, g, &config->border_colors.focused);
		} else if (focused_inactive == c) {
			render_title_bar(c, cr, g, &config->border_colors.focused_inactive);
		} else {
			render_title_bar(c, cr, g, &config->border_colors.unfocused);
		}
	}
}

void update_view_border(swayc_t *view) {
	if (!view->visible) {
		return;
	}

	cairo_t *cr = NULL;
	cairo_surface_t *surface = NULL;

	// clear previous border buffer.
	border_clear(view->border);

	// get focused and focused_inactive views
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

	// for tabbed/stacked layouts the focused view has to draw all the
	// titlebars of the hidden views.
	swayc_t *p = NULL;
	if (view->parent->focused == view && (p = swayc_tabbed_stacked_ancestor(view))) {
		struct wlc_geometry g = {
			.origin = {
				.x = p->x,
				.y = p->y
			},
			.size = {
				.w = p->width,
				.h = p->height
			}
		};
		cr = create_border_buffer(view, g, &surface);
		if (view == focused) {
			render_borders(view, cr, &config->border_colors.focused, false);
		} else {
			render_borders(view, cr, &config->border_colors.focused_inactive, false);
		}

		// generate container titles
		int i;
		for (i = 0; i < p->children->length; ++i) {
			swayc_t *child = p->children->items[i];
			if (child->type == C_CONTAINER) {
				generate_container_title(child);
			}
		}

		update_tabbed_stacked_titlebars(p, cr, &g, focused, focused_inactive);
	} else {
		switch (view->border_type) {
		case B_NONE:
			break;
		case B_PIXEL:
			cr = create_border_buffer(view, view->border_geometry, &surface);
			if (!cr) {
				break;
			}

			if (focused == view) {
				render_borders(view, cr, &config->border_colors.focused, true);
			} else if (focused_inactive == view) {
				render_borders(view, cr, &config->border_colors.focused_inactive, true);
			} else {
				render_borders(view, cr, &config->border_colors.unfocused, true);
			}

			break;
		case B_NORMAL:
			cr = create_border_buffer(view, view->border_geometry, &surface);
			if (!cr) {
				break;
			}

			if (focused == view) {
				render_borders(view, cr, &config->border_colors.focused, false);
				render_title_bar(view, cr, &view->border_geometry,
					&config->border_colors.focused);
			} else if (focused_inactive == view) {
				render_borders(view, cr, &config->border_colors.focused_inactive, false);
				render_title_bar(view, cr, &view->border_geometry,
					&config->border_colors.focused_inactive);
			} else {
				render_borders(view, cr, &config->border_colors.unfocused, false);
				render_title_bar(view, cr, &view->border_geometry,
					&config->border_colors.unfocused);
			}

			break;
		}
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


	// emulate i3 behavior for drawing borders for tabbed and stacked layouts:
	// if we are not the only child in the container, always draw borders,
	// regardless of the border setting on the individual view
	if (!c || (c->border_type == B_NONE
			&& !((c->parent->layout == L_TABBED || c->parent->layout == L_STACKED)
				&& c->parent->children->length > 1))) {
		return;
	}

	if (c->border && c->border->buffer) {
		wlc_pixels_write(WLC_RGBA8888, &c->border->geometry, c->border->buffer);
	}
}
