#ifndef _SWAY_NAGBAR_TYPES_H
#define _SWAY_NAGBAR_TYPES_H

struct sway_nagbar_type {
	char *name;
	uint32_t button_background;
	uint32_t background;
	uint32_t text;
	uint32_t border;
	uint32_t border_bottom;
};

void nagbar_types_add_default(list_t *types);

struct sway_nagbar_type *nagbar_type_get(list_t *types, char *name);

struct sway_nagbar_type *nagbar_type_clone(struct sway_nagbar_type *type);

void nagbar_type_free(struct sway_nagbar_type *type);

void nagbar_types_free(list_t *types);

int nagbar_parse_type(int argc, char **argv, struct sway_nagbar_type *type);

#endif
