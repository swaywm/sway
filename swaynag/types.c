#define _XOPEN_SOURCE 500
#include <getopt.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include "list.h"
#include "swaynag/config.h"
#include "swaynag/types.h"
#include "util.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

void swaynag_types_add_default(list_t *types) {
	struct swaynag_type *type_defaults;
	type_defaults = calloc(1, sizeof(struct swaynag_type));
	type_defaults->name = strdup("<defaults>");
	type_defaults->font = strdup("pango:Monospace 10");
	type_defaults->anchors = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
		| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	type_defaults->bar_border_thickness = 2;
	type_defaults->message_padding = 8;
	type_defaults->details_border_thickness = 3;
	type_defaults->button_border_thickness = 3;
	type_defaults->button_gap = 20;
	type_defaults->button_gap_close = 15;
	type_defaults->button_margin_right = 2;
	type_defaults->button_padding = 3;
	list_add(types, type_defaults);

	struct swaynag_type *type_error;
	type_error = calloc(1, sizeof(struct swaynag_type));
	type_error->name = strdup("error");
	type_error->button_background = 0x680A0AFF;
	type_error->background = 0x900000FF;
	type_error->text = 0xFFFFFFFF;
	type_error->border = 0xD92424FF;
	type_error->border_bottom = 0x470909FF;
	list_add(types, type_error);

	struct swaynag_type *type_warning;
	type_warning = calloc(1, sizeof(struct swaynag_type));
	type_warning->name = strdup("warning");
	type_warning->button_background = 0xFFC100FF;
	type_warning->background = 0xFFA800FF;
	type_warning->text = 0x000000FF;
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

	if (!dest->font && src->font) {
		dest->font = strdup(src->font);
	}

	if (!dest->output && src->output) {
		dest->output = strdup(src->output);
	}

	if (dest->anchors == 0 && src->anchors > 0) {
		dest->anchors = src->anchors;
	}

	// Colors
	if (dest->button_background == 0 && src->button_background > 0) {
		dest->button_background = src->button_background;
	}

	if (dest->background == 0 && src->background > 0) {
		dest->background = src->background;
	}

	if (dest->text == 0 && src->text > 0) {
		dest->text = src->text;
	}

	if (dest->border == 0 && src->border > 0) {
		dest->border = src->border;
	}

	if (dest->border_bottom == 0 && src->border_bottom > 0) {
		dest->border_bottom = src->border_bottom;
	}

	// Sizing
	if (dest->bar_border_thickness == 0 && src->bar_border_thickness > 0) {
		dest->bar_border_thickness = src->bar_border_thickness;
	}

	if (dest->message_padding == 0 && src->message_padding > 0) {
		dest->message_padding = src->message_padding;
	}

	if (dest->details_border_thickness == 0
			&& src->details_border_thickness > 0) {
		dest->details_border_thickness = src->details_border_thickness;
	}

	if (dest->button_border_thickness == 0
			&& src->button_border_thickness > 0) {
		dest->button_border_thickness = src->button_border_thickness;
	}

	if (dest->button_gap == 0 && src->button_gap > 0) {
		dest->button_gap = src->button_gap;
	}

	if (dest->button_gap_close == 0 && src->button_gap_close > 0) {
		dest->button_gap_close = src->button_gap_close;
	}

	if (dest->button_margin_right == 0 && src->button_margin_right > 0) {
		dest->button_margin_right = src->button_margin_right;
	}

	if (dest->button_padding == 0 && src->button_padding > 0) {
		dest->button_padding = src->button_padding;
	}
}

void swaynag_type_free(struct swaynag_type *type) {
	free(type->name);
	free(type->font);
	free(type->output);
	free(type);
}

void swaynag_types_free(list_t *types) {
	while (types->length) {
		struct swaynag_type *type = types->items[0];
		swaynag_type_free(type);
		list_del(types, 0);
	}
	list_free(types);
}
