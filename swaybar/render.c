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
	static const int ws_horizontal_padding = 5;
	static const double ws_vertical_padding = 1.5;
	static const int ws_spacing = 1;

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
	uint32_t ideal_height = ws_vertical_padding * 2 + text_height;
	if (height < ideal_height) {
		height = ideal_height;
	}
	uint32_t width = ws_horizontal_padding * 2 + text_width;

	cairo_set_source_u32(cairo, box_colors.background);
	cairo_rectangle(cairo, *x, 0, width - 1, height);
	cairo_fill(cairo);

	cairo_set_source_u32(cairo, box_colors.border);
	cairo_rectangle(cairo, *x, 0, width - 1, height);
	cairo_stroke(cairo);

	double text_y = height / 2.0 - text_height / 2.0;
	cairo_set_source_u32(cairo, box_colors.text);
	cairo_move_to(cairo, (int)*x + ws_horizontal_padding, (int)floor(text_y));
	pango_printf(cairo, config->font, 1, true, "%s", name);

	*x += width + ws_spacing;
	return ideal_height;
}

static void update_heights(uint32_t height, uint32_t *min, uint32_t *max) {
	if (*min < height) {
		*min = height;
	}
	if (height > *max) {
		*max = height;
	}
}

static uint32_t render_to_cairo(cairo_t *cairo,
		struct swaybar *bar, struct swaybar_output *output) {
	struct swaybar_config *config = bar->config;

	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);
	cairo_restore(cairo);

	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	if (output->focused) {
		cairo_set_source_u32(cairo, config->colors.focused_background);
	} else {
		cairo_set_source_u32(cairo, config->colors.background);
	}
	cairo_paint(cairo);

	uint32_t min_height = output->height, max_height = output->height;

	double x = 0;
	if (config->workspace_buttons) {
		struct swaybar_workspace *ws;
		wl_list_for_each(ws, &output->workspaces, link) {
			uint32_t h = render_workspace_button(
					cairo, config, ws, &x, output->height);
			update_heights(h, &min_height, &max_height);
		}
	}

	// TODO: Shrink via min_height if sane
	return max_height;
}

void render_frame(struct swaybar *bar,
		struct swaybar_output *output) {
	cairo_surface_t *recorder = cairo_recording_surface_create(
			CAIRO_CONTENT_COLOR_ALPHA, NULL);
	cairo_t *cairo = cairo_create(recorder);
	uint32_t height = render_to_cairo(cairo, bar, output);
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
