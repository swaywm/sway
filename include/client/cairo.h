#ifndef _SWAY_CAIRO_H
#define _SWAY_CAIRO_H

#include <stdint.h>
#include <cairo/cairo.h>

void cairo_set_source_u32(cairo_t *cairo, uint32_t color);

cairo_surface_t *cairo_image_surface_scale(cairo_surface_t *image, int width, int height);

#ifdef WITH_GDK_PIXBUF
#include <gdk-pixbuf/gdk-pixbuf.h>

cairo_surface_t* gdk_cairo_image_surface_create_from_pixbuf(const GdkPixbuf *gdkbuf);
#endif //WITH_GDK_PIXBUF

#endif
