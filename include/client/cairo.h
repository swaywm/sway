#ifndef _SWAY_CAIRO_H
#define _SWAY_CAIRO_H

#include <gdk-pixbuf/gdk-pixbuf.h>

cairo_surface_t* gdk_cairo_image_surface_create_from_pixbuf(const GdkPixbuf *gdkbuf);

#endif
