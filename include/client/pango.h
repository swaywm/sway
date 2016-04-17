#ifndef _SWAY_CLIENT_PANGO_H
#define _SWAY_CLIENT_PANGO_H

#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdarg.h>
#include <stdbool.h>

PangoLayout *get_pango_layout(cairo_t *cairo, const char *font, const char *text, bool markup);
void get_text_size(cairo_t *cairo, const char *font, int *width, int *height,
		bool markup, const char *fmt, ...);
void pango_printf(cairo_t *cairo, const char *font, bool markup, const char *fmt, ...);

#endif
