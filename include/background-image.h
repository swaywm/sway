#ifndef _SWAY_BACKGROUND_IMAGE_H
#define _SWAY_BACKGROUND_IMAGE_H
#include "cairo_util.h"

enum background_mode {
	BACKGROUND_MODE_STRETCH,
	BACKGROUND_MODE_FILL,
	BACKGROUND_MODE_FIT,
	BACKGROUND_MODE_CENTER,
	BACKGROUND_MODE_TILE,
	BACKGROUND_MODE_SOLID_COLOR,
	BACKGROUND_MODE_INVALID,
};

enum background_mode parse_background_mode(const char *mode);
cairo_surface_t *load_background_image(const char *path);
void render_background_image(cairo_t *cairo, cairo_surface_t *image,
		enum background_mode mode, int buffer_width, int buffer_height);

#endif
