#ifndef _SWAYNAG_TYPES_H
#define _SWAYNAG_TYPES_H

struct swaynag_type {
	char *name;
	uint32_t button_background;
	uint32_t background;
	uint32_t text;
	uint32_t border;
	uint32_t border_bottom;
};

void swaynag_types_add_default(list_t *types);

struct swaynag_type *swaynag_type_get(list_t *types, char *name);

struct swaynag_type *swaynag_type_clone(struct swaynag_type *type);

void swaynag_type_free(struct swaynag_type *type);

void swaynag_types_free(list_t *types);

int swaynag_parse_type(int argc, char **argv, struct swaynag_type *type);

#endif
