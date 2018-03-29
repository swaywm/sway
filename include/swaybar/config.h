#ifndef _SWAYBAR_CONFIG_H
#define _SWAYBAR_CONFIG_H
#include <stdbool.h>
#include <stdint.h>
#include "list.h"
#include "util.h"

/**
 * Colors for a box with background, border and text colors.
 */
struct box_colors {
	uint32_t border;
	uint32_t background;
	uint32_t text;
};

/**
 * Swaybar config.
 */
struct swaybar_config {
	char *status_command;
	bool pango_markup;
	uint32_t position; // zwlr_layer_surface_v1_anchor
	char *font;
	char *sep_symbol;
	char *mode;
	bool strip_workspace_numbers;
	bool binding_mode_indicator;
	bool wrap_scroll;
	bool workspace_buttons;
	bool all_outputs;
	list_t *outputs;
	int height;

	struct {
		uint32_t background;
		uint32_t statusline;
		uint32_t separator;

		uint32_t focused_background;
		uint32_t focused_statusline;
		uint32_t focused_separator;

		struct box_colors focused_workspace;
		struct box_colors active_workspace;
		struct box_colors inactive_workspace;
		struct box_colors urgent_workspace;
		struct box_colors binding_mode;
	} colors;
};

struct swaybar_config *init_config();
void free_config(struct swaybar_config *config);

#endif
