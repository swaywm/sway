#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "log.h"

PangoLayout *get_pango_layout(cairo_t *cairo, const char *font, const char *text) {
	PangoLayout *layout = pango_cairo_create_layout(cairo);
	pango_layout_set_text(layout, text, -1);
	PangoFontDescription *desc = pango_font_description_from_string(font);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_single_paragraph_mode(layout, 1);
	pango_font_description_free(desc);
	return layout;
}

void get_text_size(cairo_t *cairo, const char *font, int *width, int *height, const char *fmt, ...) {
	char *buf = malloc(2048);

	va_list args;
	va_start(args, fmt);
	if (vsnprintf(buf, 2048, fmt, args) >= 2048) {
		strcpy(buf, "[buffer overflow]");
	}
	va_end(args);

	PangoLayout *layout = get_pango_layout(cairo, font, buf);
	pango_cairo_update_layout(cairo, layout);

	pango_layout_get_pixel_size(layout, width, height);

	g_object_unref(layout);

	free(buf);
}

void pango_printf(cairo_t *cairo, const char *font, const char *fmt, ...) {
	char *buf = malloc(2048);

	va_list args;
	va_start(args, fmt);
	if (vsnprintf(buf, 2048, fmt, args) >= 2048) {
		strcpy(buf, "[buffer overflow]");
	}
	va_end(args);

	PangoLayout *layout = get_pango_layout(cairo, font, buf);
	pango_cairo_update_layout(cairo, layout);

	pango_cairo_show_layout(cairo, layout);

	g_object_unref(layout);

	free(buf);
}
