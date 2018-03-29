#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PangoLayout *get_pango_layout(cairo_t *cairo, const char *font,
		const char *text, int32_t scale, bool markup) {
	PangoLayout *layout = pango_cairo_create_layout(cairo);
	PangoAttrList *attrs;
	if (markup) {
		char *buf;
		pango_parse_markup(text, -1, 0, &attrs, &buf, NULL, NULL);
		pango_layout_set_markup(layout, buf, -1);
		free(buf);
	} else {
		attrs = pango_attr_list_new();
		pango_layout_set_text(layout, text, -1);
	}
	pango_attr_list_insert(attrs, pango_attr_scale_new(scale));
	PangoFontDescription *desc = pango_font_description_from_string(font);
	pango_layout_set_font_description(layout, desc);
	pango_layout_set_single_paragraph_mode(layout, 1);
	pango_layout_set_attributes(layout, attrs);
	pango_attr_list_unref(attrs);
	pango_font_description_free(desc);
	return layout;
}

void get_text_size(cairo_t *cairo, const char *font, int *width, int *height,
		int32_t scale, bool markup, const char *fmt, ...) {
	char *buf = malloc(2048);

	va_list args;
	va_start(args, fmt);
	if (vsnprintf(buf, 2048, fmt, args) >= 2048) {
		strcpy(buf, "[buffer overflow]");
	}
	va_end(args);

	PangoLayout *layout = get_pango_layout(cairo, font, buf, scale, markup);
	pango_cairo_update_layout(cairo, layout);
	pango_layout_get_pixel_size(layout, width, height);
	g_object_unref(layout);
	free(buf);
}

void pango_printf(cairo_t *cairo, const char *font,
		int32_t scale, bool markup, const char *fmt, ...) {
	char *buf = malloc(2048);

	va_list args;
	va_start(args, fmt);
	if (vsnprintf(buf, 2048, fmt, args) >= 2048) {
		strcpy(buf, "[buffer overflow]");
	}
	va_end(args);

	PangoLayout *layout = get_pango_layout(cairo, font, buf, scale, markup);
	pango_cairo_update_layout(cairo, layout);
	pango_cairo_show_layout(cairo, layout);
	g_object_unref(layout);
	free(buf);
}
