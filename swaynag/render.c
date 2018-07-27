#include <stdint.h>
#include "cairo.h"
#include "log.h"
#include "pango.h"
#include "pool-buffer.h"
#include "swaynag/nagbar.h"
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
	cairo_move_to(cairo, padding, (int)(ideal_height - text_height) / 2);
	pango_printf(cairo, nagbar->font, nagbar->scale, true, "%s",
			nagbar->message);

	return ideal_height;
}

static void render_details_scroll_button(cairo_t *cairo,
		struct sway_nagbar *nagbar, struct sway_nagbar_button *button) {
	int text_width, text_height;
	get_text_size(cairo, nagbar->font, &text_width, &text_height,
			nagbar->scale, true, "%s", button->text);

	int border = NAGBAR_BUTTON_BORDER_THICKNESS * nagbar->scale;
	int padding = NAGBAR_BUTTON_PADDING * nagbar->scale;

	cairo_set_source_u32(cairo, nagbar->colors.border);
	cairo_rectangle(cairo, button->x, button->y,
			button->width, button->height);
	cairo_fill(cairo);

	cairo_set_source_u32(cairo, nagbar->colors.button_background);
	cairo_rectangle(cairo, button->x + border, button->y + border,
			button->width - (border * 2), button->height - (border * 2));
	cairo_fill(cairo);

	cairo_set_source_u32(cairo, nagbar->colors.text);
	cairo_move_to(cairo, button->x + border + padding,
			button->y + border + (button->height - text_height) / 2);
	pango_printf(cairo, nagbar->font, nagbar->scale, true, "%s", button->text);
}

static int get_detailed_scroll_button_width(cairo_t *cairo,
		struct sway_nagbar *nagbar) {
	int up_width, down_width, temp_height;
	get_text_size(cairo, nagbar->font, &up_width, &temp_height,
			nagbar->scale, true, "%s", nagbar->details.button_up.text);
	get_text_size(cairo, nagbar->font, &down_width, &temp_height,
			nagbar->scale, true, "%s", nagbar->details.button_down.text);

	int text_width =  up_width > down_width ? up_width : down_width;
	int border = NAGBAR_BUTTON_BORDER_THICKNESS * nagbar->scale;
	int padding = NAGBAR_BUTTON_PADDING * nagbar->scale;

	return text_width + border * 2 + padding * 2;
}

static uint32_t render_detailed(cairo_t *cairo, struct sway_nagbar *nagbar,
		uint32_t y) {
	uint32_t width = nagbar->width * nagbar->scale;
	uint32_t height = nagbar->height * nagbar->scale;
	height -= NAGBAR_BAR_BORDER_THICKNESS * nagbar->scale;

	int border = NAGBAR_DETAILS_BORDER_THICKNESS * nagbar->scale;
	int padding = NAGBAR_MESSAGE_PADDING * nagbar->scale;
	int decor = padding + border;

	nagbar->details.x = decor;
	nagbar->details.y = y + decor;
	nagbar->details.width = width - decor * 2;

	PangoLayout *layout = get_pango_layout(cairo, nagbar->font,
			nagbar->details.message, nagbar->scale, true);
	pango_layout_set_width(layout,
			(nagbar->details.width - padding * 2) * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_single_paragraph_mode(layout, false);
	pango_cairo_update_layout(cairo, layout);
	nagbar->details.total_lines = pango_layout_get_line_count(layout);

	PangoLayoutLine *line;
	line = pango_layout_get_line_readonly(layout, nagbar->details.offset);
	gint offset = line->start_index;
	const char *text = pango_layout_get_text(layout);
	pango_layout_set_text(layout, text + offset, strlen(text) - offset);

	int text_width, text_height;
	pango_cairo_update_layout(cairo, layout);
	pango_layout_get_pixel_size(layout, &text_width, &text_height);

	bool show_buttons = nagbar->details.offset > 0;
	int button_width = get_detailed_scroll_button_width(cairo, nagbar);
	if (show_buttons) {
		nagbar->details.width -= button_width;
		pango_layout_set_width(layout,
				(nagbar->details.width - padding * 2) * PANGO_SCALE);
	}

	uint32_t ideal_height;
	do {
		ideal_height = nagbar->details.y + text_height + decor + padding * 2;
		if (ideal_height > NAGBAR_MAX_HEIGHT) {
			ideal_height = NAGBAR_MAX_HEIGHT;

			if (!show_buttons) {
				show_buttons = true;
				nagbar->details.width -= button_width;
				pango_layout_set_width(layout,
						(nagbar->details.width - padding * 2) * PANGO_SCALE);
			}
		}

		nagbar->details.height = ideal_height - nagbar->details.y - decor;
		pango_layout_set_height(layout,
				(nagbar->details.height - padding * 2) * PANGO_SCALE);
		pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
		pango_cairo_update_layout(cairo, layout);
		pango_layout_get_pixel_size(layout, &text_width, &text_height);
	} while (text_height != (nagbar->details.height - padding * 2));

	nagbar->details.visible_lines = pango_layout_get_line_count(layout);

	if (show_buttons) {
		nagbar->details.button_up.x =
			nagbar->details.x + nagbar->details.width;
		nagbar->details.button_up.y = nagbar->details.y;
		nagbar->details.button_up.width = button_width;
		nagbar->details.button_up.height = nagbar->details.height / 2;
		render_details_scroll_button(cairo, nagbar,
				&nagbar->details.button_up);

		nagbar->details.button_down.x =
			nagbar->details.x + nagbar->details.width;
		nagbar->details.button_down.y =
			nagbar->details.button_up.y + nagbar->details.button_up.height;
		nagbar->details.button_down.width = button_width;
		nagbar->details.button_down.height = nagbar->details.height / 2;
		render_details_scroll_button(cairo, nagbar,
				&nagbar->details.button_down);
	}

	cairo_set_source_u32(cairo, nagbar->colors.border);
	cairo_rectangle(cairo, nagbar->details.x, nagbar->details.y,
			nagbar->details.width, nagbar->details.height);
	cairo_fill(cairo);

	cairo_move_to(cairo, nagbar->details.x + padding,
			nagbar->details.y + padding);
	cairo_set_source_u32(cairo, nagbar->colors.text);
	pango_cairo_show_layout(cairo, layout);
	g_object_unref(layout);

	return ideal_height;
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
	button->y = (int)(ideal_height - text_height) / 2 - padding;
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

	return ideal_height;
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

	if (nagbar->details.visible) {
		h = render_detailed(cairo, nagbar, max_height);
		max_height = h > max_height ? h : max_height;
	}

	int border = NAGBAR_BAR_BORDER_THICKNESS * nagbar->scale;
	if (max_height > nagbar->height) {
		max_height += border;
	}
	cairo_set_source_u32(cairo, nagbar->colors.border_bottom);
	cairo_rectangle(cairo, 0, nagbar->height * nagbar->scale - border,
			nagbar->width * nagbar->scale, border);
	cairo_fill(cairo);

	return max_height;
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
		if (!nagbar->current_buffer) {
			wlr_log(WLR_DEBUG, "Failed to get buffer. Skipping frame.");
			goto cleanup;
		}

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

cleanup:
	cairo_surface_destroy(recorder);
	cairo_destroy(cairo);
}
