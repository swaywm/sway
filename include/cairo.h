#ifndef _SWAY_CAIRO_H
#define _SWAY_CAIRO_H

#include "config.h"
#include <stdint.h>
#include <cairo/cairo.h>
#include <wlr/types/wlr_output.h>
#if HAVE_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>
#endif

void cairo_set_source_u32(cairo_t *cairo, uint32_t color);
cairo_subpixel_order_t to_cairo_subpixel_order(enum wl_output_subpixel subpixel);

cairo_surface_t *cairo_image_surface_scale(cairo_surface_t *image,
		int width, int height);

#if HAVE_GDK_PIXBUF

cairo_surface_t* gdk_cairo_image_surface_create_from_pixbuf(
		const GdkPixbuf *gdkbuf);

#endif // HAVE_GDK_PIXBUF

#endif
