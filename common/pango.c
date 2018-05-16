#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"

int escape_markup_text(const char *src, char *dest, int dest_length) {
	int length = 0;

	while (src[0]) {
		switch (src[0]) {
		case '&':
			length += 5;
			if (dest && dest_length - length >= 0) {
				dest += sprintf(dest, "%s", "&amp;");
			} else {
				dest_length = -1;
			}
			break;
		case '<':
			length += 4;
			if (dest && dest_length - length >= 0) {
				dest += sprintf(dest, "%s", "&lt;");
			} else {
				dest_length = -1;
			}
			break;
		case '>':
			length += 4;
			if (dest && dest_length - length >= 0) {
				dest += sprintf(dest, "%s", "&gt;");
			} else {
				dest_length = -1;
			}
			break;
		case '\'':
			length += 6;
			if (dest && dest_length - length >= 0) {
				dest += sprintf(dest, "%s", "&apos;");
			} else {
				dest_length = -1;
			}
			break;
		case '"':
			length += 6;
			if (dest && dest_length - length >= 0) {
				dest += sprintf(dest, "%s", "&quot;");
			} else {
				dest_length = -1;
			}
			break;
		default:
			length += 1;
			if (dest && dest_length - length >= 0) {
				*(dest++) = *src;
			} else {
				dest_length = -1;
			}
		}
		src++;
	}
	// if we could not fit the escaped string in dest, return -1
	if (dest && dest_length == -1) {
		return -1;
	}
	return length;
}

PangoLayout *get_pango_layout(cairo_t *cairo, const char *font,
		const char *text, double scale, bool markup) {
	PangoLayout *layout = pango_cairo_create_layout(cairo);
	PangoAttrList *attrs;
	if (markup) {
		char *buf;
		GError *error = NULL;
		if (pango_parse_markup(text, -1, 0, &attrs, &buf, NULL, &error)) {
			pango_layout_set_markup(layout, buf, -1);
			free(buf);
		} else {
			wlr_log(L_ERROR, "pango_parse_markup '%s' -> error %s", text,
					error->message);
			g_error_free(error);
			markup = false; // fallback to plain text
		}
	}
	if (!markup) {
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
		double scale, bool markup, const char *fmt, ...) {
	static char buf[2048];

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
}

void pango_printf(cairo_t *cairo, const char *font,
		double scale, bool markup, const char *fmt, ...) {
	static char buf[2048];

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
}
