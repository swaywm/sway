#define _POSIX_C_SOURCE 200809L
#include <assert.h>
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
#include "swaybar/i3bar.h"
#include "swaybar/ipc.h"
#include "swaybar/render.h"
#include "swaybar/status_line.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static const int WS_HORIZONTAL_PADDING = 5;
static const double WS_VERTICAL_PADDING = 1.5;
static const double BORDER_WIDTH = 1;

static uint32_t render_status_line_error(cairo_t *cairo,
		struct swaybar_output *output, double *x) {
	const char *error = output->bar->status->text;
	if (!error) {
		return 0;
	}

	uint32_t height = output->height * output->scale;

	cairo_set_source_u32(cairo, 0xFF0000FF);

	int margin = 3 * output->scale;
	int ws_vertical_padding = WS_VERTICAL_PADDING * output->scale;

	char *font = output->bar->config->font;
	int text_width, text_height;
	get_text_size(cairo, font, &text_width, &text_height, NULL,
			output->scale, false, "%s", error);

	uint32_t ideal_height = text_height + ws_vertical_padding * 2;
	uint32_t ideal_surface_height = ideal_height / output->scale;
	if (output->height < ideal_surface_height) {
		return ideal_surface_height;
	}
	*x -= text_width + margin;

	double text_y = height / 2.0 - text_height / 2.0;
	cairo_move_to(cairo, *x, (int)floor(text_y));
	pango_printf(cairo, font, output->scale, false, "%s", error);
	*x -= margin;
	return output->height;
}

static uint32_t render_status_line_text(cairo_t *cairo,
		struct swaybar_output *output, double *x) {
	const char *text = output->bar->status->text;
	if (!text) {
		return 0;
	}

	struct swaybar_config *config = output->bar->config;
	cairo_set_source_u32(cairo, output->focused ?
			config->colors.focused_statusline : config->colors.statusline);

	int text_width, text_height;
	get_text_size(cairo, config->font, &text_width, &text_height, NULL,
			output->scale, config->pango_markup, "%s", text);

	int ws_vertical_padding = WS_VERTICAL_PADDING * output->scale;
	int margin = 3 * output->scale;

	uint32_t ideal_height = text_height + ws_vertical_padding * 2;
	uint32_t ideal_surface_height = ideal_height / output->scale;
	if (output->height < ideal_surface_height) {
		return ideal_surface_height;
	}

	*x -= text_width + margin;
	uint32_t height = output->height * output->scale;
	double text_y = height / 2.0 - text_height / 2.0;
	cairo_move_to(cairo, *x, (int)floor(text_y));
	pango_printf(cairo, config->font, output->scale,
			config->pango_markup, "%s", text);
	*x -= margin;
	return output->height;
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

static enum hotspot_event_handling block_hotspot_callback(struct swaybar_output *output,
			int x, int y, enum x11_button button, void *data) {
	struct i3bar_block *block = data;
	struct status_line *status = output->bar->status;
	return i3bar_block_send_click(status, block, x, y, button);
}

static void i3bar_block_unref_callback(void *data) {
	i3bar_block_unref(data);
}

static uint32_t render_status_block(cairo_t *cairo,
		struct swaybar_output *output, struct i3bar_block *block, double *x,
		bool edge) {
	if (!block->full_text || !*block->full_text) {
		return 0;
	}

	struct swaybar_config *config = output->bar->config;

	int text_width, text_height;
	get_text_size(cairo, config->font, &text_width, &text_height, NULL,
			output->scale, block->markup, "%s", block->full_text);

	int margin = 3 * output->scale;
	int ws_vertical_padding = WS_VERTICAL_PADDING * 2;

	int width = text_width;
	if (width < block->min_width) {
		width = block->min_width;
	}

	double block_width = width;
	uint32_t ideal_height = text_height + ws_vertical_padding * 2;
	uint32_t ideal_surface_height = ideal_height / output->scale;
	if (output->height < ideal_surface_height) {
		return ideal_surface_height;
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

	int sep_width, sep_height;
	if (!edge) {
		if (config->sep_symbol) {
			get_text_size(cairo, config->font, &sep_width, &sep_height, NULL,
					output->scale, false, "%s", config->sep_symbol);
			uint32_t _ideal_height = sep_height + ws_vertical_padding * 2;
			uint32_t _ideal_surface_height = _ideal_height / output->scale;
			if (output->height < _ideal_surface_height) {
				return _ideal_surface_height;
			}
			if (sep_width > block->separator_block_width) {
				block->separator_block_width = sep_width + margin * 2;
			}
		}
		*x -= block->separator_block_width;
	} else {
		*x -= margin;
	}

	uint32_t height = output->height * output->scale;
	if (output->bar->status->click_events) {
		struct swaybar_hotspot *hotspot = calloc(1, sizeof(struct swaybar_hotspot));
		hotspot->x = *x;
		hotspot->y = 0;
		hotspot->width = width;
		hotspot->height = height;
		hotspot->callback = block_hotspot_callback;
		hotspot->destroy = i3bar_block_unref_callback;
		hotspot->data = block;
		block->ref_count++;
		wl_list_insert(&output->hotspots, &hotspot->link);
	}

	double pos = *x;
	if (block->background) {
		cairo_set_source_u32(cairo, block->background);
		cairo_rectangle(cairo, pos - 0.5 * output->scale,
				output->scale, width, height);
		cairo_fill(cairo);
	}

	if (block->border && block->border_top > 0) {
		render_sharp_line(cairo, block->border,
				pos - 0.5 * output->scale, output->scale,
				block_width, block->border_top);
	}
	if (block->border && block->border_bottom > 0) {
		render_sharp_line(cairo, block->border,
				pos - 0.5 * output->scale,
				height - output->scale - block->border_bottom,
				block_width, block->border_bottom);
	}
	if (block->border != 0 && block->border_left > 0) {
		render_sharp_line(cairo, block->border,
				pos - 0.5 * output->scale, output->scale,
				block->border_left, height);
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
	pango_printf(cairo, config->font, output->scale,
			block->markup, "%s", block->full_text);
	pos += width;

	if (block->border && block->border_right > 0) {
		pos += margin;
		render_sharp_line(cairo, block->border,
				pos - 0.5 * output->scale, output->scale,
				block->border_right, height);
		pos += block->border_right;
	}

	if (!edge && block->separator) {
		if (output->focused) {
			cairo_set_source_u32(cairo, config->colors.focused_separator);
		} else {
			cairo_set_source_u32(cairo, config->colors.separator);
		}
		if (config->sep_symbol) {
			offset = pos + (block->separator_block_width - sep_width) / 2;
			cairo_move_to(cairo, offset, height / 2.0 - sep_height / 2.0);
			pango_printf(cairo, config->font, output->scale, false,
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
	return output->height;
}

static uint32_t render_status_line_i3bar(cairo_t *cairo,
		struct swaybar_output *output, double *x) {
	uint32_t max_height = 0;
	bool edge = true;
	struct i3bar_block *block;
	wl_list_for_each(block, &output->bar->status->blocks, link) {
		uint32_t h = render_status_block(cairo, output, block, x, edge);
		max_height = h > max_height ? h : max_height;
		edge = false;
	}
	return max_height;
}

static uint32_t render_status_line(cairo_t *cairo,
		struct swaybar_output *output, double *x) {
	struct status_line *status = output->bar->status;
	switch (status->protocol) {
	case PROTOCOL_ERROR:
		return render_status_line_error(cairo, output, x);
	case PROTOCOL_TEXT:
		return render_status_line_text(cairo, output, x);
	case PROTOCOL_I3BAR:
		return render_status_line_i3bar(cairo, output, x);
	case PROTOCOL_UNDEF:
		return 0;
	}
	return 0;
}

static uint32_t render_binding_mode_indicator(cairo_t *cairo,
		struct swaybar_output *output, double x) {
	const char *mode = output->bar->mode;
	if (!mode) {
		return 0;
	}

	struct swaybar_config *config = output->bar->config;
	int text_width, text_height;
	get_text_size(cairo, config->font, &text_width, &text_height, NULL,
			output->scale, output->bar->mode_pango_markup,
			"%s", mode);

	int ws_vertical_padding = WS_VERTICAL_PADDING * output->scale;
	int ws_horizontal_padding = WS_HORIZONTAL_PADDING * output->scale;
	int border_width = BORDER_WIDTH * output->scale;

	uint32_t ideal_height = text_height + ws_vertical_padding * 2
		+ border_width * 2;
	uint32_t ideal_surface_height = ideal_height / output->scale;
	if (output->height < ideal_surface_height) {
		return ideal_surface_height;
	}
	uint32_t width = text_width + ws_horizontal_padding * 2 + border_width * 2;

	uint32_t height = output->height * output->scale;
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
	pango_printf(cairo, config->font, output->scale,
			output->bar->mode_pango_markup, "%s", mode);
	return output->height;
}

static enum hotspot_event_handling workspace_hotspot_callback(struct swaybar_output *output,
			int x, int y, enum x11_button button, void *data) {
	if (button != LEFT) {
		return HOTSPOT_PROCESS;
	}
	ipc_send_workspace_command(output->bar, (const char *)data);
	return HOTSPOT_IGNORE;
}

static uint32_t render_workspace_button(cairo_t *cairo,
		struct swaybar_output *output,
		struct swaybar_workspace *ws, double *x) {
	struct swaybar_config *config = output->bar->config;
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

	uint32_t height = output->height * output->scale;

	int text_width, text_height;
	get_text_size(cairo, config->font, &text_width, &text_height, NULL,
			output->scale, config->pango_markup, "%s", ws->label);

	int ws_vertical_padding = WS_VERTICAL_PADDING * output->scale;
	int ws_horizontal_padding = WS_HORIZONTAL_PADDING * output->scale;
	int border_width = BORDER_WIDTH * output->scale;

	uint32_t ideal_height = ws_vertical_padding * 2 + text_height
		+ border_width * 2;
	uint32_t ideal_surface_height = ideal_height / output->scale;
	if (output->height < ideal_surface_height) {
		return ideal_surface_height;
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
	pango_printf(cairo, config->font, output->scale, config->pango_markup,
			"%s", ws->label);

	struct swaybar_hotspot *hotspot = calloc(1, sizeof(struct swaybar_hotspot));
	hotspot->x = *x;
	hotspot->y = 0;
	hotspot->width = width;
	hotspot->height = height;
	hotspot->callback = workspace_hotspot_callback;
	hotspot->destroy = free;
	hotspot->data = strdup(ws->name);
	wl_list_insert(&output->hotspots, &hotspot->link);

	*x += width;
	return output->height;
}

static uint32_t render_to_cairo(cairo_t *cairo, struct swaybar_output *output) {
	struct swaybar *bar = output->bar;
	struct swaybar_config *config = bar->config;
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	if (output->focused) {
		cairo_set_source_u32(cairo, config->colors.focused_background);
	} else {
		cairo_set_source_u32(cairo, config->colors.background);
	}
	cairo_paint(cairo);

	int th;
	get_text_size(cairo, config->font, NULL, &th, NULL, output->scale, false, "");
	uint32_t max_height = (th + WS_VERTICAL_PADDING * 4) / output->scale;
	/*
	 * Each render_* function takes the actual height of the bar, and returns
	 * the ideal height. If the actual height is too short, the render function
	 * can do whatever it wants - the buffer won't be committed. If the actual
	 * height is too tall, the render function should adapt its drawing to
	 * utilize the available space.
	 */
	double x = output->width * output->scale;
	if (bar->status) {
		uint32_t h = render_status_line(cairo, output, &x);
		max_height = h > max_height ? h : max_height;
	}
	x = 0;
	if (config->workspace_buttons) {
		struct swaybar_workspace *ws;
		wl_list_for_each(ws, &output->workspaces, link) {
			uint32_t h = render_workspace_button(cairo, output, ws, &x);
			max_height = h > max_height ? h : max_height;
		}
	}
	if (config->binding_mode_indicator) {
		uint32_t h = render_binding_mode_indicator(cairo, output, x);
		max_height = h > max_height ? h : max_height;
	}

	return max_height > output->height ? max_height : output->height;
}

static void output_frame_handle_done(void *data, struct wl_callback *callback,
		uint32_t time) {
	wl_callback_destroy(callback);
	struct swaybar_output *output = data;
	output->frame_scheduled = false;
	if (output->dirty) {
		render_frame(output);
		output->dirty = false;
	}
}

static const struct wl_callback_listener output_frame_listener = {
	.done = output_frame_handle_done
};

void render_frame(struct swaybar_output *output) {
	assert(output->surface != NULL);
	if (!output->layer_surface) {
		return;
	}

	free_hotspots(&output->hotspots);

	cairo_surface_t *recorder = cairo_recording_surface_create(
			CAIRO_CONTENT_COLOR_ALPHA, NULL);
	cairo_t *cairo = cairo_create(recorder);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);
	cairo_font_options_t *fo = cairo_font_options_create();
	cairo_font_options_set_hint_style(fo, CAIRO_HINT_STYLE_FULL);
	cairo_font_options_set_antialias(fo, CAIRO_ANTIALIAS_SUBPIXEL);
	cairo_font_options_set_subpixel_order(fo, to_cairo_subpixel_order(output->subpixel));
	cairo_set_font_options(cairo, fo);
	cairo_font_options_destroy(fo);
	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);
	cairo_restore(cairo);
	uint32_t height = render_to_cairo(cairo, output);
	int config_height = output->bar->config->height;
	if (config_height >= 0 && height < (uint32_t)config_height) {
		height = config_height;
	}
	if (height != output->height || output->width == 0) {
		// Reconfigure surface
		zwlr_layer_surface_v1_set_size(output->layer_surface, 0, height);
		zwlr_layer_surface_v1_set_margin(output->layer_surface,
				output->bar->config->gaps.top,
				output->bar->config->gaps.right,
				output->bar->config->gaps.bottom,
				output->bar->config->gaps.left);
		if (strcmp(output->bar->config->mode, "dock") == 0) {
			zwlr_layer_surface_v1_set_exclusive_zone(output->layer_surface, height);
		}
		// TODO: this could infinite loop if the compositor assigns us a
		// different height than what we asked for
		wl_surface_commit(output->surface);
	} else if (height > 0) {
		// Replay recording into shm and send it off
		output->current_buffer = get_next_buffer(output->bar->shm,
				output->buffers,
				output->width * output->scale,
				output->height * output->scale);
		if (!output->current_buffer) {
			cairo_surface_destroy(recorder);
			cairo_destroy(cairo);
			return;
		}
		cairo_t *shm = output->current_buffer->cairo;

		cairo_save(shm);
		cairo_set_operator(shm, CAIRO_OPERATOR_CLEAR);
		cairo_paint(shm);
		cairo_restore(shm);

		cairo_set_source_surface(shm, recorder, 0.0, 0.0);
		cairo_paint(shm);

		wl_surface_set_buffer_scale(output->surface, output->scale);
		wl_surface_attach(output->surface,
				output->current_buffer->buffer, 0, 0);
		wl_surface_damage(output->surface, 0, 0,
				output->width, output->height);

		struct wl_callback *frame_callback = wl_surface_frame(output->surface);
		wl_callback_add_listener(frame_callback, &output_frame_listener, output);
		output->frame_scheduled = true;

		wl_surface_commit(output->surface);
	}
	cairo_surface_destroy(recorder);
	cairo_destroy(cairo);
}
