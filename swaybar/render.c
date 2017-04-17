#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "client/cairo.h"
#include "client/pango.h"
#include "client/window.h"
#include "swaybar/config.h"
#include "swaybar/status_line.h"
#include "swaybar/render.h"
#include "log.h"


/* internal spacing */
static int margin = 3;
static int ws_horizontal_padding = 5;
static double ws_vertical_padding = 1.5;
static int ws_spacing = 1;

/**
 * Renders a sharp line of any width and height.
 *
 * The line is drawn from (x,y) to (x+width,y+height) where width/height is 0
 * if the line has a width/height of one pixel, respectively.
 */
static void render_sharp_line(cairo_t *cairo, uint32_t color, double x, double y, double width, double height) {
	cairo_set_source_u32(cairo, color);

	if (width > 1 && height > 1) {
		cairo_rectangle(cairo, x, y, width, height);
		cairo_fill(cairo);
	} else {
		if (width == 1) {
			x += 0.5;
			height += y;
			width = x;
		}

		if (height == 1) {
			y += 0.5;
			width += x;
			height = y;
		}

		cairo_move_to(cairo, x, y);
		cairo_set_line_width(cairo, 1.0);
		cairo_line_to(cairo, width, height);
		cairo_stroke(cairo);
	}
}

static void render_block(struct window *window, struct config *config, struct status_block *block, double *x, bool edge, bool is_focused) {
	int width, height, sep_width;
	get_text_size(window->cairo, window->font, &width, &height,
			window->scale, block->markup, "%s", block->full_text);

	int textwidth = width;
	double block_width = width;

	if (width < block->min_width) {
		width = block->min_width;
	}

	*x -= width;

	if (block->border != 0 && block->border_left > 0) {
		*x -= (block->border_left + margin);
		block_width += block->border_left + margin;
	}

	if (block->border != 0 && block->border_right > 0) {
		*x -= (block->border_right + margin);
		block_width += block->border_right + margin;
	}

	// Add separator
	if (!edge) {
		if (config->sep_symbol) {
			get_text_size(window->cairo, window->font, &sep_width, &height,
					window->scale, false, "%s", config->sep_symbol);
			if (sep_width > block->separator_block_width) {
				block->separator_block_width = sep_width + margin * 2;
			}
		}

		*x -= block->separator_block_width;
	} else {
		*x -= margin;
	}

	double pos = *x;

	// render background
	if (block->background != 0x0) {
		cairo_set_source_u32(window->cairo, block->background);
		cairo_rectangle(window->cairo, pos - 0.5, 1, block_width, (window->height * window->scale) - 2);
		cairo_fill(window->cairo);
	}

	// render top border
	if (block->border != 0 && block->border_top > 0) {
		render_sharp_line(window->cairo, block->border,
				pos - 0.5,
				1,
				block_width,
				block->border_top);
	}

	// render bottom border
	if (block->border != 0 && block->border_bottom > 0) {
		render_sharp_line(window->cairo, block->border,
				pos - 0.5,
				(window->height * window->scale) - 1 - block->border_bottom,
				block_width,
				block->border_bottom);
	}

	// render left border
	if (block->border != 0 && block->border_left > 0) {
		render_sharp_line(window->cairo, block->border,
				pos - 0.5,
				1,
				block->border_left,
				(window->height * window->scale) - 2);

		pos += block->border_left + margin;
	}

	// render text
	double offset = 0;

	if (strncmp(block->align, "left", 5) == 0) {
		offset = pos;
	} else if (strncmp(block->align, "right", 5) == 0) {
		offset = pos + width - textwidth;
	} else if (strncmp(block->align, "center", 6) == 0) {
		offset = pos + (width - textwidth) / 2;
	}

	cairo_move_to(window->cairo, offset, margin);
	cairo_set_source_u32(window->cairo, block->color);
	pango_printf(window->cairo, window->font, window->scale,
			block->markup, "%s", block->full_text);

	pos += width;

	// render right border
	if (block->border != 0 && block->border_right > 0) {
		pos += margin;

		render_sharp_line(window->cairo, block->border,
				pos - 0.5,
				1,
				block->border_right,
				(window->height * window->scale) - 2);

		pos += block->border_right;
	}

	// render separator
	if (!edge && block->separator) {
		if (is_focused) {
			cairo_set_source_u32(window->cairo, config->colors.focused_separator);
		} else {
			cairo_set_source_u32(window->cairo, config->colors.separator);
		}
		if (config->sep_symbol) {
			offset = pos + (block->separator_block_width - sep_width) / 2;
			cairo_move_to(window->cairo, offset, margin);
			pango_printf(window->cairo, window->font, window->scale,
					false, "%s", config->sep_symbol);
		} else {
			cairo_set_line_width(window->cairo, 1);
			cairo_move_to(window->cairo, pos + block->separator_block_width/2,
					margin);
			cairo_line_to(window->cairo, pos + block->separator_block_width/2,
					(window->height * window->scale) - margin);
			cairo_stroke(window->cairo);
		}
	}

}

static const char *strip_workspace_name(bool strip_num, const char *ws_name) {
	bool strip = false;
	int i;

	if (strip_num) {
		int len = strlen(ws_name);
		for (i = 0; i < len; ++i) {
			if (!('0' <= ws_name[i] && ws_name[i] <= '9')) {
				if (':' == ws_name[i] && i < len-1 && i > 0) {
					strip = true;
					++i;
				}
				break;
			}
		}
	}

	if (strip) {
		return ws_name + i;
	}

	return ws_name;
}

void workspace_button_size(struct window *window, const char *workspace_name, int *width, int *height) {
	const char *stripped_name = strip_workspace_name(swaybar.config->strip_workspace_numbers, workspace_name);

	get_text_size(window->cairo, window->font, width, height,
			window->scale, true, "%s", stripped_name);
	*width += 2 * ws_horizontal_padding;
	*height += 2 * ws_vertical_padding;
}

static void render_workspace_button(struct window *window, struct config *config, struct workspace *ws, double *x) {
	const char *stripped_name = strip_workspace_name(config->strip_workspace_numbers, ws->name);

	struct box_colors box_colors;
	if (ws->urgent) {
		box_colors = config->colors.urgent_workspace;
	} else if (ws->focused) {
		box_colors = config->colors.focused_workspace;
	} else if (ws->visible) {
		box_colors = config->colors.active_workspace;
	} else {
		box_colors = config->colors.inactive_workspace;
	}

	int width, height;
	workspace_button_size(window, stripped_name, &width, &height);

	// background
	cairo_set_source_u32(window->cairo, box_colors.background);
	cairo_rectangle(window->cairo, *x, 1.5, width - 1, height);
	cairo_fill(window->cairo);

	// border
	cairo_set_source_u32(window->cairo, box_colors.border);
	cairo_rectangle(window->cairo, *x, 1.5, width - 1, height);
	cairo_stroke(window->cairo);

	// text
	cairo_set_source_u32(window->cairo, box_colors.text);
	cairo_move_to(window->cairo, (int)*x + ws_horizontal_padding, margin);
	pango_printf(window->cairo, window->font, window->scale,
			true, "%s", stripped_name);

	*x += width + ws_spacing;
}

static void render_binding_mode_indicator(struct window *window, struct config *config, double pos) {
	int width, height;
	get_text_size(window->cairo, window->font, &width, &height,
			window->scale, false, "%s", config->mode);

	// background
	cairo_set_source_u32(window->cairo, config->colors.binding_mode.background);
	cairo_rectangle(window->cairo, pos, 1.5, width + ws_horizontal_padding * 2 - 1,
			height + ws_vertical_padding * 2);
	cairo_fill(window->cairo);

	// border
	cairo_set_source_u32(window->cairo, config->colors.binding_mode.border);
	cairo_rectangle(window->cairo, pos, 1.5, width + ws_horizontal_padding * 2 - 1,
			height + ws_vertical_padding * 2);
	cairo_stroke(window->cairo);

	// text
	cairo_set_source_u32(window->cairo, config->colors.binding_mode.text);
	cairo_move_to(window->cairo, (int)pos + ws_horizontal_padding, margin);
	pango_printf(window->cairo, window->font, window->scale,
			false, "%s", config->mode);
}

void render(struct output *output, struct config *config, struct status_line *line) {
	int i;

	struct window *window = output->window;
	cairo_t *cairo = window->cairo;
	bool is_focused = output->focused;

	// Clear
	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);
	cairo_restore(cairo);
	// Could also be done by always rendering then conditionally clearing and drawing alpha
	// That may be preferable down the line
	if (!(config->display_mode == MODE_HIDE) || config->hidden_state == BAR_SHOW) {
	
		cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	
		// Background
		if (is_focused) {
			cairo_set_source_u32(cairo, config->colors.focused_background);
		} else {
			cairo_set_source_u32(cairo, config->colors.background);
		}
		cairo_paint(cairo);
	
		// Command output
		if (is_focused) {
			cairo_set_source_u32(cairo, config->colors.focused_statusline);
		} else {
			cairo_set_source_u32(cairo, config->colors.statusline);
		}
	
		int width, height;
	
		if (line->protocol == TEXT) {
			get_text_size(window->cairo, window->font, &width, &height,
					window->scale, config->pango_markup, "%s", line->text_line);
			cairo_move_to(cairo, (window->width * window->scale)
					- margin - width, margin);
			pango_printf(window->cairo, window->font, window->scale,
					config->pango_markup, "%s", line->text_line);
		} else if (line->protocol == I3BAR && line->block_line) {
			double pos = (window->width * window->scale) - 0.5;
			bool edge = true;
			for (i = line->block_line->length - 1; i >= 0; --i) {
				struct status_block *block = line->block_line->items[i];
				if (block->full_text && block->full_text[0]) {
					render_block(window, config, block, &pos, edge, is_focused);
					edge = false;
				}
			}
		}
	
		cairo_set_line_width(cairo, 1.0);
		double x = 0.5;
	
		// Workspaces
		if (config->workspace_buttons) {
			for (i = 0; i < output->workspaces->length; ++i) {
				struct workspace *ws = output->workspaces->items[i];
				render_workspace_button(window, config, ws, &x);
			}
		}
	
		// binding mode indicator
		if (config->mode && config->binding_mode_indicator) {
			render_binding_mode_indicator(window, config, x);
		}
	} else {
		cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
		cairo_set_source_u32(cairo, 0x00000000);
		cairo_paint(cairo);
	}
}

void set_window_height(struct window *window, int height) {
	int text_width, text_height;
	get_text_size(window->cairo, window->font,
			&text_width, &text_height, window->scale, false,
			"Test string for measuring purposes");
	if (height > 0) {
		margin = (height - text_height) / 2;
		ws_vertical_padding = margin - 1.5;
	}
	window->height = (text_height + margin * 2) / window->scale;
}
