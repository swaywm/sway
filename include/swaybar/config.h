#ifndef _SWAYBAR_CONFIG_H
#define _SWAYBAR_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

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
struct config {
	char *status_command;
	bool pango_markup;
	uint32_t position;
	char *font;
	char *sep_symbol;
	char *mode;
	char *display_mode;
	char *hidden_state;
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

/**
 * Parse position top|bottom|left|right.
 */
uint32_t parse_position(const char *position);

/**
 * Parse font.
 */
char *parse_font(const char *font);

/**
 * Initialize default sway config.
 */
struct config *init_config();

/**
 * Free config struct.
 */
void free_config(struct config *config);

#endif /* _SWAYBAR_CONFIG_H */
