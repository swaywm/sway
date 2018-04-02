#define _POSIX_C_SOURCE 200809L
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wlr/util/log.h>
#include "cairo.h"
#include "pango.h"
#include "pool-buffer.h"
#include "swaybar/bar.h"
#include "swaybar/config.h"
#include "swaybar/ipc.h"
#include "swaybar/render.h"
#include "swaybar/status_line.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static const int ws_horizontal_padding = 5;
static const double ws_vertical_padding = 1.5;
static const double border_width = 1;

static uint32_t render_status_line_error(cairo_t *cairo,
		struct swaybar_config *config, const char *error,
		double *x, uint32_t width, uint32_t height) {
	if (!error) {
		return 0;
	}
	cairo_set_source_u32(cairo, 0xFF0000FF);
	static const int margin = 3;
	int text_width, text_height;
	get_text_size(cairo, config->font,
			&text_width, &text_height, 1, false, "%s", error);
	uint32_t ideal_height = text_height + ws_vertical_padding * 2;
	if (height < ideal_height) {
		return ideal_height;
	}
	*x -= text_width + margin;
	double text_y = height / 2.0 - text_height / 2.0;
	cairo_move_to(cairo, *x, (int)floor(text_y));
	pango_printf(cairo, config->font, 1, false, "%s", error);
	*x -= margin;
	return ideal_height;
}

static uint32_t render_status_line_text(cairo_t *cairo,
		struct swaybar_config *config, const char *text,
		bool focused, double *x, uint32_t width, uint32_t height) {
	if (!text) {
		return 0;
	}
	cairo_set_source_u32(cairo, focused ?
			config->colors.focused_statusline : config->colors.statusline);
	static const int margin = 3;
	int text_width, text_height;
	get_text_size(cairo, config->font, &text_width, &text_height,
			1, config->pango_markup, "%s", text);
	uint32_t ideal_height = text_height + ws_vertical_padding * 2;
	if (height < ideal_height) {
		return ideal_height;
	}
	*x -= text_width + margin;
	double text_y = height / 2.0 - text_height / 2.0;
	cairo_move_to(cairo, *x, (int)floor(text_y));
	pango_printf(cairo, config->font, 1, config->pango_markup, "%s", text);
	*x -= margin;
	return ideal_height;
}

static void render_sharp_line(cairo_t *cairo, uint32_t color,
		double x, double y, double width, double height) {
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

static void block_hotspot_callback(struct swaybar_output *output,
			int x, int y, uint32_t button, void *data) {
	struct i3bar_block *block = data;
	struct status_line *status = output->bar->status;
	i3bar_block_send_click(status, block, x, y, button);
}

static uint32_t render_status_block(cairo_t *cairo,
		struct swaybar_config *config, struct swaybar_output *output,
		struct i3bar_block *block, double *x,
		uint32_t height, bool focused, bool edge) {
	static const int margin = 3;
	if (!block->full_text || !*block->full_text) {
		return 0;
	}

	int text_width, text_height;
	get_text_size(cairo, config->font, &text_width, &text_height,
			1, block->markup, "%s", block->full_text);
	int width = text_width;
	if (width < block->min_width) {
		width = block->min_width;
	}
	double block_width = width;
	uint32_t ideal_height = text_height + ws_vertical_padding * 2;
	if (height < ideal_height) {
		return ideal_height;
	}

	*x -= width;
	if (block->border && block->border_left > 0) {
		*x -= (block->border_left + margin);
		block_width += block->border_left + margin;
	}
	if (block->border && block->border_right > 0) {
		*x -= (block->border_right + margin);
		block_width += block->border_right + margin;
	}

	int sep_width;
	if (!edge) {
		if (config->sep_symbol) {
			int _height;
			get_text_size(cairo, config->font, &sep_width, &_height,
					1, false, "%s", config->sep_symbol);
			uint32_t _ideal_height = _height + ws_vertical_padding * 2;
			if (height < _ideal_height) {
				return _height;
			}
			if (sep_width > block->separator_block_width) {
				block->separator_block_width = sep_width + margin * 2;
			}
		}
		*x -= block->separator_block_width;
	} else {
		*x -= margin;
	}

	struct swaybar_hotspot *hotspot = calloc(1, sizeof(struct swaybar_hotspot));
	hotspot->x = *x;
	hotspot->y = 0;
	hotspot->width = width;
	hotspot->height = height;
	hotspot->callback = block_hotspot_callback;
	hotspot->destroy = NULL;
	hotspot->data = block;
	wl_list_insert(&output->hotspots, &hotspot->link);

	double pos = *x;
	if (block->background) {
		cairo_set_source_u32(cairo, block->background);
		cairo_rectangle(cairo, pos - 0.5, 1, block_width, height);
		cairo_fill(cairo);
	}

	if (block->border && block->border_top > 0) {
		render_sharp_line(cairo, block->border,
				pos - 0.5, 1, block_width, block->border_top);
	}
	if (block->border && block->border_bottom > 0) {
		render_sharp_line(cairo, block->border,
				pos - 0.5, height - 1 - block->border_bottom,
				block_width, block->border_bottom);
	}
	if (block->border != 0 && block->border_left > 0) {
		render_sharp_line(cairo, block->border,
				pos - 0.5, 1, block->border_left, height);
		pos += block->border_left + margin;
	}

	double offset = 0;
	if (strncmp(block->align, "left", 5) == 0) {
		offset = pos;
	} else if (strncmp(block->align, "right", 5) == 0) {
		offset = pos + width - text_width;
	} else if (strncmp(block->align, "center", 6) == 0) {
		offset = pos + (width - text_width) / 2;
	}
	cairo_move_to(cairo, offset, height / 2.0 - text_height / 2.0);
	uint32_t color = block->color ?  *block->color : config->colors.statusline;
	cairo_set_source_u32(cairo, color);
	pango_printf(cairo, config->font, 1, block->markup, "%s", block->full_text);
	pos += width;

	if (block->border && block->border_right > 0) {
		pos += margin;
		render_sharp_line(cairo, block->border,
				pos - 0.5, 1, block->border_right, height);
		pos += block->border_right;
	}

	if (!edge && block->separator) {
		if (focused) {
			cairo_set_source_u32(cairo, config->colors.focused_separator);
		} else {
			cairo_set_source_u32(cairo, config->colors.separator);
		}
		if (config->sep_symbol) {
			offset = pos + (block->separator_block_width - sep_width) / 2;
			cairo_move_to(cairo, offset, margin);
			pango_printf(cairo, config->font, 1, false,
					"%s", config->sep_symbol);
		} else {
			cairo_set_line_width(cairo, 1);
			cairo_move_to(cairo,
					pos + block->separator_block_width / 2, margin);
			cairo_line_to(cairo,
					pos + block->separator_block_width / 2, height - margin);
			cairo_stroke(cairo);
		}
	}
	return ideal_height;
}

static uint32_t render_status_line_i3bar(cairo_t *cairo,
		struct swaybar_config *config, struct swaybar_output *output,
		struct status_line *status, bool focused,
		double *x, uint32_t width, uint32_t height) {
	uint32_t max_height = 0;
	bool edge = true;
	struct i3bar_block *block;
	wl_list_for_each_reverse(block, &status->blocks, link) {
		uint32_t h = render_status_block(cairo, config, output,
				block, x, height, focused, edge);
		max_height = h > max_height ? h : max_height;
		edge = false;
	}
	return max_height;
}

static uint32_t render_status_line(cairo_t *cairo,
		struct swaybar_config *config, struct swaybar_output *output,
		struct status_line *status, bool focused,
		double *x, uint32_t width, uint32_t height) {
	switch (status->protocol) {
	case PROTOCOL_ERROR:
		return render_status_line_error(cairo,
				config, status->text, x, width, height);
	case PROTOCOL_TEXT:
		return render_status_line_text(cairo,
				config, status->text, focused, x, width, height);
	case PROTOCOL_I3BAR:
		return render_status_line_i3bar(cairo, config, output, status,
				focused, x, width, height);
	case PROTOCOL_UNDEF:
		return 0;
	}
	return 0;
}

static uint32_t render_binding_mode_indicator(cairo_t *cairo,
		struct swaybar_config *config, const char *mode, double x,
		uint32_t height) {
	int text_width, text_height;
	get_text_size(cairo, config->font, &text_width, &text_height,
			1, true, "%s", mode);
	uint32_t ideal_height = text_height + ws_vertical_padding * 2
		+ border_width * 2;
	if (height < ideal_height) {
		return ideal_height;
	}
	uint32_t width = text_width + ws_horizontal_padding * 2 + border_width * 2;

	cairo_set_source_u32(cairo, config->colors.binding_mode.background);
	cairo_rectangle(cairo, x, 0, width, height);
	cairo_fill(cairo);

	cairo_set_source_u32(cairo, config->colors.binding_mode.border);
	cairo_rectangle(cairo, x, 0, width, border_width);
	cairo_fill(cairo);
	cairo_rectangle(cairo, x, 0, border_width, height);
	cairo_fill(cairo);
	cairo_rectangle(cairo, x + width - border_width, 0, border_width, height);
	cairo_fill(cairo);
	cairo_rectangle(cairo, x, height - border_width, width, border_width);
	cairo_fill(cairo);

	double text_y = height / 2.0 - text_height / 2.0;
	cairo_set_source_u32(cairo, config->colors.binding_mode.text);
	cairo_move_to(cairo, x + width / 2 - text_width / 2, (int)floor(text_y));
	pango_printf(cairo, config->font, 1, true, "%s", mode);
	return ideal_height;
}

static const char *strip_workspace_number(const char *ws_name) {
	size_t len = strlen(ws_name);
	for (size_t i = 0; i < len; ++i) {
		if (ws_name[i] < '0' || ws_name[i] > '9') {
			if (':' == ws_name[i] && i < len - 1 && i > 0) {
				return ws_name + i + 1;
			}
			return ws_name;
		}
	}
	return ws_name;
}

static void workspace_hotspot_callback(struct swaybar_output *output,
			int x, int y, uint32_t button, void *data) {
	ipc_send_workspace_command(output->bar, (const char *)data);
}

static uint32_t render_workspace_button(cairo_t *cairo,
		struct swaybar_output *output, struct swaybar_config *config,
		struct swaybar_workspace *ws, double *x, uint32_t height) {
	const char *name = ws->name;
	if (config->strip_workspace_numbers) {
		name = strip_workspace_number(ws->name);
	}

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

	int text_width, text_height;
	get_text_size(cairo, config->font, &text_width, &text_height,
			1, true, "%s", name);
	uint32_t ideal_height = ws_vertical_padding * 2 + text_height
		+ border_width * 2;
	if (height < ideal_height) {
		return ideal_height;
	}
	uint32_t width = ws_horizontal_padding * 2 + text_width + border_width * 2;

	cairo_set_source_u32(cairo, box_colors.background);
	cairo_rectangle(cairo, *x, 0, width, height);
	cairo_fill(cairo);

	cairo_set_source_u32(cairo, box_colors.border);
	cairo_rectangle(cairo, *x, 0, width, border_width);
	cairo_fill(cairo);
	cairo_rectangle(cairo, *x, 0, border_width, height);
	cairo_fill(cairo);
	cairo_rectangle(cairo, *x + width - border_width, 0, border_width, height);
	cairo_fill(cairo);
	cairo_rectangle(cairo, *x, height - border_width, width, border_width);
	cairo_fill(cairo);

	double text_y = height / 2.0 - text_height / 2.0;
	cairo_set_source_u32(cairo, box_colors.text);
	cairo_move_to(cairo, *x + width / 2 - text_width / 2, (int)floor(text_y));
	pango_printf(cairo, config->font, 1, true, "%s", name);

	struct swaybar_hotspot *hotspot = calloc(1, sizeof(struct swaybar_hotspot));
	hotspot->x = *x;
	hotspot->y = 0;
	hotspot->width = width;
	hotspot->height = height;
	hotspot->callback = workspace_hotspot_callback;
	hotspot->destroy = free;
	hotspot->data = strdup(name);
	wl_list_insert(&output->hotspots, &hotspot->link);

	*x += width;
	return height;
}

static uint32_t render_to_cairo(cairo_t *cairo,
		struct swaybar *bar, struct swaybar_output *output) {
	struct swaybar_config *config = bar->config;
	wlr_log(L_DEBUG, "output %p", output);

	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	if (output->focused) {
		cairo_set_source_u32(cairo, config->colors.focused_background);
	} else {
		cairo_set_source_u32(cairo, config->colors.background);
	}
	cairo_paint(cairo);

	uint32_t max_height = 0;
	/*
	 * Each render_* function takes the actual height of the bar, and returns
	 * the ideal height. If the actual height is too short, the render function
	 * can do whatever it wants - the buffer won't be committed. If the actual
	 * height is too tall, the render function should adapt its drawing to
	 * utilize the available space.
	 */
	double x = 0;
	if (config->workspace_buttons) {
		struct swaybar_workspace *ws;
		wl_list_for_each_reverse(ws, &output->workspaces, link) {
			uint32_t h = render_workspace_button(cairo,
					output, config, ws, &x, output->height);
			max_height = h > max_height ? h : max_height;
		}
	}
	if (config->binding_mode_indicator && config->mode) {
		uint32_t h = render_binding_mode_indicator(
				cairo, config, config->mode, x, output->height);
		max_height = h > max_height ? h : max_height;
	}
	x = output->width;
	if (bar->status) {
		uint32_t h = render_status_line(cairo, config, output, bar->status,
				output->focused, &x, output->width, output->height);
		max_height = h > max_height ? h : max_height;
	}

	return max_height > output->height ? max_height : output->height;
}

void render_frame(struct swaybar *bar, struct swaybar_output *output) {
	struct swaybar_hotspot *hotspot, *tmp;
	wl_list_for_each_safe(hotspot, tmp, &output->hotspots, link) {
		if (hotspot->destroy) {
			hotspot->destroy(hotspot->data);
		}
		wl_list_remove(&hotspot->link);
		free(hotspot);
	}

	cairo_surface_t *recorder = cairo_recording_surface_create(
			CAIRO_CONTENT_COLOR_ALPHA, NULL);
	cairo_t *cairo = cairo_create(recorder);
	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);
	cairo_restore(cairo);
	uint32_t height = render_to_cairo(cairo, bar, output);
	if (bar->config->height >= 0 && height < (uint32_t)bar->config->height) {
		height = bar->config->height;
	}
	if (height != output->height) {
		// Reconfigure surface
		zwlr_layer_surface_v1_set_size(
				output->layer_surface, 0, height);
		zwlr_layer_surface_v1_set_exclusive_zone(output->layer_surface, height);
		// TODO: this could infinite loop if the compositor assigns us a
		// different height than what we asked for
		wl_surface_commit(output->surface);
		wl_display_roundtrip(bar->display);
	} else {
		// Replay recording into shm and send it off
		output->current_buffer = get_next_buffer(bar->shm,
				output->buffers, output->width, output->height);
		cairo_t *shm = output->current_buffer->cairo;

		cairo_save(shm);
		cairo_set_operator(shm, CAIRO_OPERATOR_CLEAR);
		cairo_paint(shm);
		cairo_restore(shm);

		cairo_set_source_surface(shm, recorder, 0.0, 0.0);
		cairo_paint(shm);

		wl_surface_attach(output->surface,
				output->current_buffer->buffer, 0, 0);
		wl_surface_damage(output->surface, 0, 0, output->width, output->height);
		wl_surface_commit(output->surface);
		wl_display_roundtrip(bar->display);
	}
	cairo_surface_destroy(recorder);
	cairo_destroy(cairo);
}
