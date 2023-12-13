#ifndef _SWAY_CAIRO_UTIL_H
#define _SWAY_CAIRO_UTIL_H
#include <cairo.h>
#include <stdint.h>
#include <wayland-client-protocol.h>
#include "config.h"

void cairo_set_source_u32(cairo_t *cairo, uint32_t color);
cairo_subpixel_order_t to_cairo_subpixel_order(enum wl_output_subpixel subpixel);

cairo_surface_t *cairo_image_surface_scale(cairo_surface_t *image,
		int width, int height);

#endif
