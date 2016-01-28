#include "render.h"
#include <cairo.h>
#include <stdlib.h>

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
	unsigned char *surf_data;
	cairo_surface_t *surf;
	int texture_id;
	const struct wlc_geometry *geo = wlc_view_get_geometry(view);
	cairo_t *cr = create_cairo_context(geo->size.w, geo->size.h, 4, &surf, &surf_data);
	// TODO
	cairo_destroy(cr);
	free(surf_data);
}
