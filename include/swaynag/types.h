#ifndef _SWAYNAG_TYPES_H
#define _SWAYNAG_TYPES_H

struct swaynag_type {
	char *name;

	char *font;
	char *output;
	uint32_t anchors;

	uint32_t button_background;
	uint32_t background;
	uint32_t text;
	uint32_t border;
	uint32_t border_bottom;

	uint32_t bar_border_thickness;
	uint32_t message_padding;
	uint32_t details_border_thickness;
	uint32_t button_border_thickness;
	uint32_t button_gap;
	uint32_t button_gap_close;
	uint32_t button_margin_right;
	uint32_t button_padding;
};

void swaynag_types_add_default(list_t *types);

struct swaynag_type *swaynag_type_get(list_t *types, char *name);

struct swaynag_type *swaynag_type_clone(struct swaynag_type *type);

void swaynag_type_merge(struct swaynag_type *dest, struct swaynag_type *src);

void swaynag_type_free(struct swaynag_type *type);

void swaynag_types_free(list_t *types);

#endif
