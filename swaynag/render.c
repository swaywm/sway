#include <stdint.h>
#include "cairo_util.h"
#include "log.h"
#include "pango.h"
#include "pool-buffer.h"
#include "swaynag/swaynag.h"
#include "swaynag/types.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static uint32_t render_message(cairo_t *cairo, struct swaynag *swaynag) {
	int text_width, text_height;
	get_text_size(cairo, swaynag->type->font_description, &text_width, &text_height, NULL,
			1, true, "%s", swaynag->message);

	int padding = swaynag->type->message_padding;

	uint32_t ideal_height = text_height + padding * 2;
	uint32_t ideal_surface_height = ideal_height;
	if (swaynag->height < ideal_surface_height) {
		return ideal_surface_height;
	}

	cairo_set_source_u32(cairo, swaynag->type->text);
	cairo_move_to(cairo, padding, (int)(ideal_height - text_height) / 2);
	render_text(cairo, swaynag->type->font_description, 1, false,
			"%s", swaynag->message);

	return ideal_surface_height;
}

static void render_details_scroll_button(cairo_t *cairo,
		struct swaynag *swaynag, struct swaynag_button *button) {
	int text_width, text_height;
	get_text_size(cairo, swaynag->type->font_description, &text_width, &text_height, NULL,
			1, true, "%s", button->text);

	int border = swaynag->type->button_border_thickness;
	int padding = swaynag->type->button_padding;

	cairo_set_source_u32(cairo, swaynag->type->details_background);
	cairo_rectangle(cairo, button->x, button->y,
			button->width, button->height);
	cairo_fill(cairo);

	cairo_set_source_u32(cairo, swaynag->type->button_background);
	cairo_rectangle(cairo, button->x + border, button->y + border,
			button->width - (border * 2), button->height - (border * 2));
	cairo_fill(cairo);

	cairo_set_source_u32(cairo, swaynag->type->button_text);
	cairo_move_to(cairo, button->x + border + padding,
			button->y + border + (button->height - text_height) / 2);
	render_text(cairo, swaynag->type->font_description, 1, true,
			"%s", button->text);
}

static int get_detailed_scroll_button_width(cairo_t *cairo,
		struct swaynag *swaynag) {
	int up_width, down_width, temp_height;
	get_text_size(cairo, swaynag->type->font_description, &up_width, &temp_height, NULL,
			1, true,
			"%s", swaynag->details.button_up.text);
	get_text_size(cairo, swaynag->type->font_description, &down_width, &temp_height, NULL,
			1, true,
			"%s", swaynag->details.button_down.text);

	int text_width =  up_width > down_width ? up_width : down_width;
	int border = swaynag->type->button_border_thickness;
	int padding = swaynag->type->button_padding;

	return text_width + border * 2 + padding * 2;
}

static uint32_t render_detailed(cairo_t *cairo, struct swaynag *swaynag,
		uint32_t y) {
	uint32_t width = swaynag->width;

	int border = swaynag->type->details_border_thickness;
	int padding = swaynag->type->message_padding;
	int decor = padding + border;

	swaynag->details.x = decor;
	swaynag->details.y = y + decor;
	swaynag->details.width = width - decor * 2;

	PangoLayout *layout = get_pango_layout(cairo, swaynag->type->font_description,
			swaynag->details.message, 1, false);
	pango_layout_set_width(layout,
			(swaynag->details.width - padding * 2) * PANGO_SCALE);
	pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_single_paragraph_mode(layout, false);
	pango_cairo_update_layout(cairo, layout);
	swaynag->details.total_lines = pango_layout_get_line_count(layout);

	PangoLayoutLine *line;
	line = pango_layout_get_line_readonly(layout, swaynag->details.offset);
	gint offset = line->start_index;
	const char *text = pango_layout_get_text(layout);
	pango_layout_set_text(layout, text + offset, strlen(text) - offset);

	int text_width, text_height;
	pango_cairo_update_layout(cairo, layout);
	pango_layout_get_pixel_size(layout, &text_width, &text_height);

	bool show_buttons = swaynag->details.offset > 0;
	int button_width = get_detailed_scroll_button_width(cairo, swaynag);
	if (show_buttons) {
		swaynag->details.width -= button_width;
		pango_layout_set_width(layout,
				(swaynag->details.width - padding * 2) * PANGO_SCALE);
	}

	uint32_t ideal_height;
	do {
		ideal_height = swaynag->details.y + text_height + decor + padding * 2;
		if (ideal_height > SWAYNAG_MAX_HEIGHT) {
			ideal_height = SWAYNAG_MAX_HEIGHT;

			if (!show_buttons) {
				show_buttons = true;
				swaynag->details.width -= button_width;
				pango_layout_set_width(layout,
						(swaynag->details.width - padding * 2) * PANGO_SCALE);
			}
		}

		swaynag->details.height = ideal_height - swaynag->details.y - decor;
		pango_layout_set_height(layout,
				(swaynag->details.height - padding * 2) * PANGO_SCALE);
		pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);
		pango_cairo_update_layout(cairo, layout);
		pango_layout_get_pixel_size(layout, &text_width, &text_height);
	} while (text_height != (swaynag->details.height - padding * 2));

	swaynag->details.visible_lines = pango_layout_get_line_count(layout);

	if (show_buttons) {
		swaynag->details.button_up.x =
			swaynag->details.x + swaynag->details.width;
		swaynag->details.button_up.y = swaynag->details.y;
		swaynag->details.button_up.width = button_width;
		swaynag->details.button_up.height = swaynag->details.height / 2;
		render_details_scroll_button(cairo, swaynag,
				&swaynag->details.button_up);

		swaynag->details.button_down.x =
			swaynag->details.x + swaynag->details.width;
		swaynag->details.button_down.y =
			swaynag->details.button_up.y + swaynag->details.button_up.height;
		swaynag->details.button_down.width = button_width;
		swaynag->details.button_down.height = swaynag->details.height / 2;
		render_details_scroll_button(cairo, swaynag,
				&swaynag->details.button_down);
	}

	cairo_set_source_u32(cairo, swaynag->type->details_background);
	cairo_rectangle(cairo, swaynag->details.x, swaynag->details.y,
			swaynag->details.width, swaynag->details.height);
	cairo_fill(cairo);

	cairo_move_to(cairo, swaynag->details.x + padding,
			swaynag->details.y + padding);
	cairo_set_source_u32(cairo, swaynag->type->text);
	pango_cairo_show_layout(cairo, layout);
	g_object_unref(layout);

	return ideal_height;
}

static uint32_t render_button(cairo_t *cairo, struct swaynag *swaynag,
		int button_index, int *x) {
	struct swaynag_button *button = swaynag->buttons->items[button_index];

	int text_width, text_height;
	get_text_size(cairo, swaynag->type->font_description, &text_width, &text_height, NULL,
			1, true, "%s", button->text);

	int border = swaynag->type->button_border_thickness;
	int padding = swaynag->type->button_padding;

	uint32_t ideal_height = text_height + padding * 2 + border * 2;
	uint32_t ideal_surface_height = ideal_height;
	if (swaynag->height < ideal_surface_height) {
		return ideal_surface_height;
	}

	button->x = *x - border - text_width - padding * 2 + 1;
	button->y = (int)(ideal_height - text_height) / 2 - padding + 1;
	button->width = text_width + padding * 2;
	button->height = text_height + padding * 2;

	cairo_set_source_u32(cairo, swaynag->type->border);
	cairo_rectangle(cairo, button->x - border, button->y - border,
			button->width + border * 2, button->height + border * 2);
	cairo_fill(cairo);

	cairo_set_source_u32(cairo, swaynag->type->button_background);
	cairo_rectangle(cairo, button->x, button->y,
			button->width, button->height);
	cairo_fill(cairo);

	cairo_set_source_u32(cairo, swaynag->type->button_text);
	cairo_move_to(cairo, button->x + padding, button->y + padding);
	render_text(cairo, swaynag->type->font_description, 1, true,
			"%s", button->text);

	*x = button->x - border;

	return ideal_surface_height;
}

static uint32_t render_to_cairo(cairo_t *cairo, struct swaynag *swaynag) {
	uint32_t max_height = 0;

	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_u32(cairo, swaynag->type->background);
	cairo_paint(cairo);

	uint32_t h = render_message(cairo, swaynag);
	max_height = h > max_height ? h : max_height;

	int x = swaynag->width - swaynag->type->button_margin_right;
	for (int i = 0; i < swaynag->buttons->length; i++) {
		h = render_button(cairo, swaynag, i, &x);
		max_height = h > max_height ? h : max_height;
		x -= swaynag->type->button_gap;
		if (i == 0) {
			x -= swaynag->type->button_gap_close;
		}
	}

	if (swaynag->details.visible) {
		h = render_detailed(cairo, swaynag, max_height);
		max_height = h > max_height ? h : max_height;
	}

	int border = swaynag->type->bar_border_thickness;
	if (max_height > swaynag->height) {
		max_height += border;
	}
	cairo_set_source_u32(cairo, swaynag->type->border_bottom);
	cairo_rectangle(cairo, 0,
			swaynag->height - border,
			swaynag->width,
			border);
	cairo_fill(cairo);

	return max_height;
}

void render_frame(struct swaynag *swaynag) {
	if (!swaynag->run_display) {
		return;
	}

	cairo_surface_t *recorder = cairo_recording_surface_create(
			CAIRO_CONTENT_COLOR_ALPHA, NULL);
	cairo_t *cairo = cairo_create(recorder);
	cairo_scale(cairo, swaynag->scale, swaynag->scale);
	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_CLEAR);
	cairo_paint(cairo);
	cairo_restore(cairo);
	uint32_t height = render_to_cairo(cairo, swaynag);
	if (height != swaynag->height) {
		zwlr_layer_surface_v1_set_size(swaynag->layer_surface, 0, height);
		zwlr_layer_surface_v1_set_exclusive_zone(swaynag->layer_surface,
				height);
		wl_surface_commit(swaynag->surface);
		wl_display_roundtrip(swaynag->display);
	} else {
		swaynag->current_buffer = get_next_buffer(swaynag->shm,
				swaynag->buffers,
				swaynag->width * swaynag->scale,
				swaynag->height * swaynag->scale);
		if (!swaynag->current_buffer) {
			sway_log(SWAY_DEBUG, "Failed to get buffer. Skipping frame.");
			goto cleanup;
		}

		cairo_t *shm = swaynag->current_buffer->cairo;
		cairo_save(shm);
		cairo_set_operator(shm, CAIRO_OPERATOR_CLEAR);
		cairo_paint(shm);
		cairo_restore(shm);
		cairo_set_source_surface(shm, recorder, 0.0, 0.0);
		cairo_paint(shm);

		wl_surface_set_buffer_scale(swaynag->surface, swaynag->scale);
		wl_surface_attach(swaynag->surface,
				swaynag->current_buffer->buffer, 0, 0);
		wl_surface_damage(swaynag->surface, 0, 0,
				swaynag->width, swaynag->height);
		wl_surface_commit(swaynag->surface);
		wl_display_roundtrip(swaynag->display);
	}

cleanup:
	cairo_surface_destroy(recorder);
	cairo_destroy(cairo);
}
