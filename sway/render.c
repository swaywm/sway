#include "render.h"
#include <wlc/wlc-render.h>
#include <cairo/cairo.h>
#include <stdlib.h>
#include <stdio.h>
#include "container.h"

void cairo_set_source_u32(cairo_t *cairo, uint32_t color) {
	cairo_set_source_rgba(cairo,
			(color >> (3*8) & 0xFF) / 255.0,
			(color >> (2*8) & 0xFF) / 255.0,
			(color >> (1*8) & 0xFF) / 255.0,
			(color >> (0*8) & 0xFF) / 255.0);
}

cairo_t *create_border_buffer(swayc_t *view, struct wlc_geometry geo,
		cairo_surface_t **surface) {
	const int channels = 4;
	cairo_t *cr;
	view->border_geometry = geo;
	view->border = calloc(channels * geo.size.w * geo.size.h,
			sizeof(unsigned char));
	if (!view->border) {
		sway_log(L_DEBUG, "Unable to allocate buffer");
		return NULL;
	}
	*surface = cairo_image_surface_create_for_data(view->border,
			CAIRO_FORMAT_ARGB32, geo.size.w, geo.size.h, channels * geo.size.w);
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

void update_view_border(swayc_t *view) {
	struct wlc_geometry geo;
	wlc_view_get_visible_geometry(view->handle, &geo);
	cairo_t *cr = NULL;
	cairo_surface_t *surface = NULL;

	if (view->border) {
		free(view->border);
		view->border = NULL;
	}

	switch (view->border_type) {
	case B_NONE:
		view->border_geometry = geo;
		break;
	case B_PIXEL:
		geo.origin.x -= view->border_thickness;
		geo.origin.y -= view->border_thickness;
		geo.size.w += view->border_thickness * 2;
		geo.size.h += view->border_thickness * 2;
		if (geo.size.w <= 0 || geo.size.h <= 0) {
			view->border = NULL;
			break;
		}
		cr = create_border_buffer(view, geo, &surface);
		if (!cr) {
			break;
		}
		cairo_set_source_u32(cr, 0x0000FFFF);
		cairo_paint(cr);
		break;
	case B_NORMAL:
		// TODO
		break;
	}
	if (surface) {
		cairo_surface_flush(surface);
		cairo_surface_destroy(surface);
	}
	if (cr) {
		cairo_destroy(cr);
		sway_log(L_DEBUG, "Created border for %p (%dx%d+%d,%d)", view,
				geo.size.w, geo.size.h, geo.origin.x, geo.origin.y);
	}
	view->border_geometry = geo;
}

void render_view_borders(wlc_handle view) {
	swayc_t *c = swayc_by_handle(view);
	if (!c || c->border_type == B_NONE) {
		return;
	}
	struct wlc_geometry geo;
	wlc_view_get_visible_geometry(view, &geo);
	if (geo.size.w != c->presumed_geometry.size.w
			|| geo.size.h != c->presumed_geometry.size.h
			|| geo.origin.x != c->presumed_geometry.origin.x
			|| geo.origin.y != c->presumed_geometry.origin.y) {
		update_view_border(c);
		c->presumed_geometry = geo;
	}
	if (c->border) {
		geo = c->border_geometry;
		sway_log(L_DEBUG, "Rendering border for %p (%dx%d+%d,%d)", c,
				geo.size.w, geo.size.h, geo.origin.x, geo.origin.y);
		wlc_pixels_write(WLC_RGBA8888, &c->border_geometry, c->border);
	}
}
