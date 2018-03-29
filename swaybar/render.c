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

static uint32_t render_to_cairo(cairo_t *cairo, struct swaybar *bar,
		struct swaybar_output *output) {
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

	// TODO: use actual height
	return 20;
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
