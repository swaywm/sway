#ifndef _SWAY_CLIENT_PANGO_H
#define _SWAY_CLIENT_PANGO_H

#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

PangoLayout *get_pango_layout(cairo_t *cairo, const char *font, const char *text,
		int32_t scale, bool markup);
void get_text_size(cairo_t *cairo, const char *font, int *width, int *height,
		int32_t scale, bool markup, const char *fmt, ...);
void pango_printf(cairo_t *cairo, const char *font, int32_t scale, bool markup, const char *fmt, ...);

#endif
