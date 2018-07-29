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

void nagbar_types_add_default(list_t *types) {
	struct sway_nagbar_type *type_error;
	type_error = calloc(1, sizeof(struct sway_nagbar_type));
	type_error->name = strdup("error");
	type_error->button_background = 0x680A0AFF;
	type_error->background = 0x900000FF;
	type_error->text = 0xFFFFFFFF;
	type_error->border = 0xD92424FF;
	type_error->border_bottom = 0x470909FF;
	list_add(types, type_error);

	struct sway_nagbar_type *type_warning;
	type_warning = calloc(1, sizeof(struct sway_nagbar_type));
	type_warning->name = strdup("warning");
	type_warning->button_background = 0xFFC100FF;
	type_warning->background = 0xFFA800FF;
	type_warning->text = 0x000000FF;
	type_warning->border = 0xAB7100FF;
	type_warning->border_bottom = 0xAB7100FF;
	list_add(types, type_warning);
}

struct sway_nagbar_type *nagbar_type_get(list_t *types, char *name) {
	for (int i = 0; i < types->length; i++) {
		struct sway_nagbar_type *type = types->items[i];
		if (strcasecmp(type->name, name) == 0) {
			return type;
		}
	}
	return NULL;
}

struct sway_nagbar_type *nagbar_type_clone(struct sway_nagbar_type *type) {
	struct sway_nagbar_type *clone;
	clone = calloc(1, sizeof(struct sway_nagbar_type));
	clone->name = strdup(type->name);
	clone->button_background = type->button_background;
	clone->background = type->background;
	clone->text = type->text;
	clone->border = type->border;
	clone->border_bottom = type->border_bottom;
	return clone;
}

void nagbar_type_free(struct sway_nagbar_type *type) {
	free(type->name);
	free(type);
}

void nagbar_types_free(list_t *types) {
	while (types->length) {
		struct sway_nagbar_type *type = types->items[0];
		nagbar_type_free(type);
		list_del(types, 0);
	}
	list_free(types);
}

int nagbar_parse_type(int argc, char **argv, struct sway_nagbar_type *type) {
	enum color_option {
		COLOR_BACKGROUND,
		COLOR_BORDER,
		COLOR_BORDER_BOTTOM,
		COLOR_BUTTON,
		COLOR_TEXT,
	};

	static struct option opts[] = {
		{"background", required_argument, NULL, COLOR_BACKGROUND},
		{"border", required_argument, NULL, COLOR_BORDER},
		{"border-bottom", required_argument, NULL, COLOR_BORDER_BOTTOM},
		{"button-background", required_argument, NULL, COLOR_BUTTON},
		{"text", required_argument, NULL, COLOR_TEXT},
		{0, 0, 0, 0}
	};

	optind = 1;
	while (1) {
		int c = getopt_long(argc, argv, "", opts, NULL);
		if (c == -1) {
			break;
		}
		switch (c) {
			case COLOR_BACKGROUND:
				type->background = parse_color(optarg);
				break;
			case COLOR_BORDER:
				type->border = parse_color(optarg);
				break;
			case COLOR_BORDER_BOTTOM:
				type->border_bottom = parse_color(optarg);
				break;
			case COLOR_BUTTON:
				type->button_background = parse_color(optarg);
				break;
			case COLOR_TEXT:
				type->text = parse_color(optarg);
				break;
			default:
				break;
		}
	}
	return 0;
}

