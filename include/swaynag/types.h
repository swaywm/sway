#ifndef _SWAYNAG_TYPES_H
#define _SWAYNAG_TYPES_H

#include <stdint.h>
#include <pango/pangocairo.h>
#include "list.h"

struct swaynag_type {
	char *name;

	PangoFontDescription *font_description;
	char *output;
	uint32_t anchors;
	int32_t layer; // enum zwlr_layer_shell_v1_layer or -1 if unset

	// Colors
	uint32_t button_text;
	uint32_t button_background;
	uint32_t details_background;
	uint32_t background;
	uint32_t text;
	uint32_t border;
	uint32_t border_bottom;

	// Sizing
	ssize_t bar_border_thickness;
	ssize_t message_padding;
	ssize_t details_border_thickness;
	ssize_t button_border_thickness;
	ssize_t button_gap;
	ssize_t button_gap_close;
	ssize_t button_margin_right;
	ssize_t button_padding;
};

struct swaynag_type *swaynag_type_new(const char *name);

void swaynag_types_add_default(list_t *types);

struct swaynag_type *swaynag_type_get(list_t *types, char *name);

struct swaynag_type *swaynag_type_clone(struct swaynag_type *type);

void swaynag_type_merge(struct swaynag_type *dest, struct swaynag_type *src);

void swaynag_type_free(struct swaynag_type *type);

void swaynag_types_free(list_t *types);

#endif
