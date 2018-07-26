#include <stdint.h>
#include "cairo.h"
#include "log.h"
#include "pango.h"
#include "pool-buffer.h"
#include "swaynagbar/nagbar.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static uint32_t render_message(cairo_t *cairo, struct sway_nagbar *nagbar) {
	uint32_t height = nagbar->height * nagbar->scale;
	height -= NAGBAR_BAR_BORDER_THICKNESS * nagbar->scale;

	int text_width, text_height;
	get_text_size(cairo, nagbar->font, &text_width, &text_height,
			nagbar->scale, true, "%s", nagbar->message);

	int padding = NAGBAR_MESSAGE_PADDING * nagbar->scale;

	uint32_t ideal_height = text_height + padding * 2;
	uint32_t ideal_surface_height = ideal_height / nagbar->scale;
	if (nagbar->height < ideal_surface_height) {
		return ideal_surface_height;
	}

	cairo_set_source_u32(cairo, nagbar->colors.text);
	cairo_move_to(cairo, padding, (int)(height / 2.0 - text_height / 2.0));
	pango_printf(cairo, nagbar->font, nagbar->scale, true, "%s",
			nagbar->message);

	return nagbar->height;
}

static uint32_t render_button(cairo_t *cairo, struct sway_nagbar *nagbar,
		int button_index, int *x) {
	uint32_t height = nagbar->height * nagbar->scale;
	height -= NAGBAR_BAR_BORDER_THICKNESS * nagbar->scale;
	struct sway_nagbar_button *button = nagbar->buttons->items[button_index];

	int text_width, text_height;
	get_text_size(cairo, nagbar->font, &text_width, &text_height,
			nagbar->scale, true, "%s", button->text);

	int border = NAGBAR_BUTTON_BORDER_THICKNESS * nagbar->scale;
	int padding = NAGBAR_BUTTON_PADDING * nagbar->scale;

	uint32_t ideal_height = text_height + padding * 2 + border * 2;
	uint32_t ideal_surface_height = ideal_height / nagbar->scale;
	if (nagbar->height < ideal_surface_height) {
		return ideal_surface_height;
	}

	button->x = *x - border - text_width - padding * 2;
	button->y = (int)(height / 2.0 - text_height / 2.0) - padding;
	button->width = text_width + padding * 2;
	button->height = text_height + padding * 2;

	cairo_set_source_u32(cairo, nagbar->colors.border);
	cairo_rectangle(cairo, button->x - border, button->y - border,
			button->width + border * 2, button->height + border * 2);
	cairo_fill(cairo);

	cairo_set_source_u32(cairo, nagbar->colors.button_background);
	cairo_rectangle(cairo, button->x, button->y,
			button->width, button->height);
	cairo_fill(cairo);

	cairo_set_source_u32(cairo, nagbar->colors.text);
	cairo_move_to(cairo, button->x + padding, button->y + padding);
	pango_printf(cairo, nagbar->font, nagbar->scale, true, "%s", button->text);

	*x = button->x - border;

	return nagbar->height;
}

static uint32_t render_to_cairo(cairo_t *cairo, struct sway_nagbar *nagbar) {
	uint32_t max_height = 0;

	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_u32(cairo, nagbar->colors.background);
	cairo_paint(cairo);

	uint32_t h = render_message(cairo, nagbar);
	max_height = h > max_height ? h : max_height;

	int x = (nagbar->width - NAGBAR_BUTTON_MARGIN_RIGHT) * nagbar->scale;
	for (int i = 0; i < nagbar->buttons->length; i++) {
		h = render_button(cairo, nagbar, i, &x);
		max_height = h > max_height ? h : max_height;
		x -= NAGBAR_BUTTON_GAP * nagbar->scale;
		if (i == 0) {
			x -= NAGBAR_BUTTON_GAP_CLOSE * nagbar->scale;
		}
	}

	int border = NAGBAR_BAR_BORDER_THICKNESS * nagbar->scale;
	if (max_height > nagbar->height) {
		max_height += border;
	}
	cairo_set_source_u32(cairo, nagbar->colors.border_bottom);
	cairo_rectangle(cairo, 0, nagbar->height * nagbar->scale - border,
			nagbar->width * nagbar->scale, border);
	cairo_fill(cairo);

	return max_height > nagbar->height ? max_height : nagbar->height;
}

void render_frame(struct sway_nagbar *nagbar) {
	if (!nagbar->run_display) {
		return;
	}

	cairo_surface_t *recorder = cairo_recording_surface_create(
			CAIRO_CONTENT_COLOR_ALPHA, NULL);
	cairo_t *cairo = cairo_create(recorder);
	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);
	cairo_restore(cairo);
	uint32_t height = render_to_cairo(cairo, nagbar);
	if (height != nagbar->height) {
		zwlr_layer_surface_v1_set_size(nagbar->layer_surface, 0, height);
		zwlr_layer_surface_v1_set_exclusive_zone(nagbar->layer_surface,
				height);
		wl_surface_commit(nagbar->surface);
		wl_display_roundtrip(nagbar->display);
	} else {
		nagbar->current_buffer = get_next_buffer(nagbar->shm,
				nagbar->buffers,
				nagbar->width * nagbar->scale,
				nagbar->height * nagbar->scale);
		cairo_t *shm = nagbar->current_buffer->cairo;

		cairo_save(shm);
		cairo_set_operator(shm, CAIRO_OPERATOR_CLEAR);
		cairo_paint(shm);
		cairo_restore(shm);

		cairo_set_source_surface(shm, recorder, 0.0, 0.0);
		cairo_paint(shm);

		wl_surface_set_buffer_scale(nagbar->surface, nagbar->scale);
		wl_surface_attach(nagbar->surface,
				nagbar->current_buffer->buffer, 0, 0);
		wl_surface_damage(nagbar->surface, 0, 0,
				nagbar->width, nagbar->height);
		wl_surface_commit(nagbar->surface);
		wl_display_roundtrip(nagbar->display);
	}
	cairo_surface_destroy(recorder);
	cairo_destroy(cairo);
}
