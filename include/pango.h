#ifndef _SWAY_PANGO_H
#define _SWAY_PANGO_H
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <cairo.h>
#include <pango/pangocairo.h>

/**
 * Utility function which escape characters a & < > ' ".
 *
 * The function returns the length of the escaped string, optionally writing the
 * escaped string to dest if provided.
 */
size_t escape_markup_text(const char *src, char *dest);
PangoLayout *get_pango_layout(cairo_t *cairo, const PangoFontDescription *desc,
		const char *text, double scale, bool markup);
void get_text_size(cairo_t *cairo, const PangoFontDescription *desc, int *width, int *height,
		int *baseline, double scale, bool markup, const char *fmt, ...);
void get_text_metrics(const PangoFontDescription *desc, int *height, int *baseline);
void render_text(cairo_t *cairo, PangoFontDescription *desc,
		double scale, bool markup, const char *fmt, ...);

#endif
