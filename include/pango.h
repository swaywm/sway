#ifndef _SWAY_PANGO_H
#define _SWAY_PANGO_H
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>

/* Utility function which escape characters a & < > ' ".
 *
 * If the dest parameter is NULL, then the function returns the length of
 * of the escaped src string. The dest_length doesn't matter.
 *
 * If the dest parameter is not NULL then the fuction escapes the src string
 * an puts the escaped string in dest and returns the lenght of the escaped string.
 * The dest_length parameter is the size of dest array. If the size of dest is not
 * enough, then the function returns -1.
 */
int escape_markup_text(const char *src, char *dest, int dest_length);
PangoLayout *get_pango_layout(cairo_t *cairo, const char *font,
		const char *text, double scale, bool markup);
void get_text_size(cairo_t *cairo, const char *font, int *width, int *height,
		double scale, bool markup, const char *fmt, ...);
void pango_printf(cairo_t *cairo, const char *font,
		double scale, bool markup, const char *fmt, ...);

#endif
