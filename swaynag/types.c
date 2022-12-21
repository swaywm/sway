#define _POSIX_C_SOURCE 200809L
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include "list.h"
#include "log.h"
#include "swaynag/config.h"
#include "swaynag/types.h"
#include "util.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

struct swaynag_type *swaynag_type_new(const char *name) {
	struct swaynag_type *type = calloc(1, sizeof(struct swaynag_type));
	if (!type) {
		sway_abort("Failed to allocate type: %s", name);
	}
	type->name = strdup(name);
	type->bar_border_thickness = -1;
	type->message_padding = -1;
	type->details_border_thickness = -1;
	type->button_border_thickness = -1;
	type->button_gap = -1;
	type->button_gap_close = -1;
	type->button_margin_right = -1;
	type->button_padding = -1;
	type->layer = -1;
	return type;
}

void swaynag_types_add_default(list_t *types) {
	struct swaynag_type *type_defaults = swaynag_type_new("<defaults>");
	type_defaults->font_description =
		pango_font_description_from_string("pango:Monospace 10");
	type_defaults->anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	type_defaults->layer = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
	type_defaults->button_background = 0x333333FF;
	type_defaults->details_background = 0x333333FF;
	type_defaults->background = 0x323232FF;
	type_defaults->text = 0xFFFFFFFF;
	type_defaults->button_text = 0xFFFFFFFF;
	type_defaults->border = 0x222222FF;
	type_defaults->border_bottom = 0x444444FF;
	type_defaults->bar_border_thickness = 2;
	type_defaults->message_padding = 8;
	type_defaults->details_border_thickness = 3;
	type_defaults->button_border_thickness = 3;
	type_defaults->button_gap = 20;
	type_defaults->button_gap_close = 15;
	type_defaults->button_margin_right = 2;
	type_defaults->button_padding = 3;
	list_add(types, type_defaults);

	struct swaynag_type *type_error = swaynag_type_new("error");
	type_error->button_background = 0x680A0AFF;
	type_error->details_background = 0x680A0AFF;
	type_error->background = 0x900000FF;
	type_error->text = 0xFFFFFFFF;
	type_error->button_text = 0xFFFFFFFF;
	type_error->border = 0xD92424FF;
	type_error->border_bottom = 0x470909FF;
	list_add(types, type_error);

	struct swaynag_type *type_warning = swaynag_type_new("warning");
	type_warning->button_background = 0xFFC100FF;
	type_warning->details_background = 0xFFC100FF;
	type_warning->background = 0xFFA800FF;
	type_warning->text = 0x000000FF;
	type_warning->button_text = 0x000000FF;
	type_warning->border = 0xAB7100FF;
	type_warning->border_bottom = 0xAB7100FF;
	list_add(types, type_warning);
}

struct swaynag_type *swaynag_type_get(list_t *types, char *name) {
	for (int i = 0; i < types->length; i++) {
		struct swaynag_type *type = types->items[i];
		if (strcasecmp(type->name, name) == 0) {
			return type;
		}
	}
	return NULL;
}

void swaynag_type_merge(struct swaynag_type *dest, struct swaynag_type *src) {
	if (!dest || !src) {
		return;
	}

	if (src->font_description) {
		dest->font_description = pango_font_description_copy(src->font_description);
	}

	if (src->output) {
		dest->output = strdup(src->output);
	}

	if (src->anchors > 0) {
		dest->anchors = src->anchors;
	}

	if (src->layer >= 0) {
		dest->layer = src->layer;
	}

	// Colors
	if (src->button_background > 0) {
		dest->button_background = src->button_background;
	}

	if (src->details_background > 0) {
		dest->details_background = src->details_background;
	}

	if (src->background > 0) {
		dest->background = src->background;
	}

	if (src->text > 0) {
		dest->text = src->text;
	}

	if (src->button_text > 0) {
		dest->button_text = src->button_text;
	}


	if (src->border > 0) {
		dest->border = src->border;
	}

	if (src->border_bottom > 0) {
		dest->border_bottom = src->border_bottom;
	}

	// Sizing
	if (src->bar_border_thickness > -1) {
		dest->bar_border_thickness = src->bar_border_thickness;
	}

	if (src->message_padding > -1) {
		dest->message_padding = src->message_padding;
	}

	if (src->details_border_thickness > -1) {
		dest->details_border_thickness = src->details_border_thickness;
	}

	if (src->button_border_thickness > -1) {
		dest->button_border_thickness = src->button_border_thickness;
	}

	if (src->button_gap > -1) {
		dest->button_gap = src->button_gap;
	}

	if (src->button_gap_close > -1) {
		dest->button_gap_close = src->button_gap_close;
	}

	if (src->button_margin_right > -1) {
		dest->button_margin_right = src->button_margin_right;
	}

	if (src->button_padding > -1) {
		dest->button_padding = src->button_padding;
	}
}

void swaynag_type_free(struct swaynag_type *type) {
	free(type->name);
	pango_font_description_free(type->font_description);
	free(type->output);
	free(type);
}

void swaynag_types_free(list_t *types) {
	for (int i = 0; i < types->length; ++i) {
		swaynag_type_free(types->items[i]);
	}
	list_free(types);
}
