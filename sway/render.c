#include "render.h"
#include <wlc/wlc-render.h>
#include <cairo/cairo.h>
#include <stdlib.h>

void cairo_set_source_u32(cairo_t *cairo, uint32_t color) {
	cairo_set_source_rgba(cairo,
			(color >> (3*8) & 0xFF) / 255.0,
			(color >> (2*8) & 0xFF) / 255.0,
			(color >> (1*8) & 0xFF) / 255.0,
			(color >> (0*8) & 0xFF) / 255.0);
}

cairo_t *create_cairo_context(int width, int height, int channels,
		cairo_surface_t **surf, unsigned char **buf) {
	cairo_t *cr;
	*buf = calloc(channels * width * height, sizeof(unsigned char));
	if (!*buf) {
		return NULL;
	}
	*surf = cairo_image_surface_create_for_data(*buf, CAIRO_FORMAT_ARGB32,
		width, height, channels * width);
	if (cairo_surface_status(*surf) != CAIRO_STATUS_SUCCESS) {
		free(*buf);
		return NULL;
	}
	cr = cairo_create(*surf);
	if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
		free(*buf);
		return NULL;
	}
	return cr;
}

void render_view_borders(wlc_handle view) {
	const int bw = 2;
	unsigned char *surf_data;
	cairo_surface_t *surf;
	struct wlc_geometry geo = *wlc_view_get_geometry(view);
	cairo_t *cr = create_cairo_context(geo.size.w + bw * 2, geo.size.h + bw * 2, 4, &surf, &surf_data);
	cairo_set_source_u32(cr, 0x0000FFFF);
	cairo_paint(cr);
	geo.origin.x -= bw;
	geo.origin.y -= bw;
	geo.size.w += bw * 2;
	geo.size.h += bw * 2;
	wlc_pixels_write(WLC_RGBA8888, &geo, surf_data);
	cairo_destroy(cr);
	free(surf_data);
}
