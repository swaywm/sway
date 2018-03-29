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
#include "swaybar/render.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static const int ws_horizontal_padding = 5;
static const double ws_vertical_padding = 1.5;
static const double border_width = 1;

static uint32_t render_binding_mode_indicator(cairo_t *cairo,
		struct swaybar_config *config, const char *mode, double x,
		uint32_t height) {
	int text_width, text_height;
	get_text_size(cairo, config->font, &text_width, &text_height,
			1, true, "%s", mode);
	uint32_t ideal_height = text_height + ws_vertical_padding * 2
		+ border_width * 2;
	if (height < ideal_height) {
		height = ideal_height;
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

static uint32_t render_workspace_button(cairo_t *cairo,
		struct swaybar_config *config, struct swaybar_workspace *ws,
		double *x, uint32_t height) {
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
		height = ideal_height;
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

	*x += width;
	return ideal_height;
}

static uint32_t render_to_cairo(cairo_t *cairo,
		struct swaybar *bar, struct swaybar_output *output) {
	struct swaybar_config *config = bar->config;

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
		wl_list_for_each(ws, &output->workspaces, link) {
			uint32_t h = render_workspace_button(
					cairo, config, ws, &x, output->height);
			max_height = h > max_height ? h : max_height;
		}
	}
	if (config->binding_mode_indicator && config->mode) {
		uint32_t h = render_binding_mode_indicator(
				cairo, config, config->mode, x, output->height);
		max_height = h > max_height ? h : max_height;
	}

	return max_height > output->height ? max_height : output->height;
}

void render_frame(struct swaybar *bar,
		struct swaybar_output *output) {
	cairo_surface_t *recorder = cairo_recording_surface_create(
			CAIRO_CONTENT_COLOR_ALPHA, NULL);
	cairo_t *cairo = cairo_create(recorder);
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
