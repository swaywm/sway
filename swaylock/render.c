#include <wayland-client.h>
#include <math.h>
#include "cairo.h"
#include "background-image.h"
#include "swaylock/swaylock.h"

#define M_PI 3.14159265358979323846

void render_frame(struct swaylock_surface *surface) {
	struct swaylock_state *state = surface->state;
	surface->current_buffer = get_next_buffer(state->shm,
			surface->buffers, surface->width, surface->height);
	cairo_t *cairo = surface->current_buffer->cairo;
	cairo_identity_matrix(cairo);
	if (state->args.mode == BACKGROUND_MODE_SOLID_COLOR) {
		cairo_set_source_u32(cairo, state->args.color);
		cairo_paint(cairo);
	} else {
		// TODO: hidpi
		render_background_image(cairo, surface->image,
				state->args.mode, surface->width, surface->height, 1);
	}
	cairo_identity_matrix(cairo);

	const int ARC_RADIUS = 50;
	const int ARC_THICKNESS = 10;
	const float TYPE_INDICATOR_RANGE = M_PI / 3.0f;
	const float TYPE_INDICATOR_BORDER_THICKNESS = M_PI / 128.0f;
	if (state->args.show_indicator && state->auth_state != AUTH_STATE_IDLE) {
		// Draw circle
		cairo_set_line_width(cairo, ARC_THICKNESS);
		cairo_arc(cairo, surface->width / 2, surface->height / 2,
				ARC_RADIUS, 0, 2 * M_PI);
		switch (state->auth_state) {
		case AUTH_STATE_INPUT:
		case AUTH_STATE_BACKSPACE: {
			cairo_set_source_rgba(cairo, 0, 0, 0, 0.75);
			cairo_fill_preserve(cairo);
			cairo_set_source_rgb(cairo, 51.0 / 255, 125.0 / 255, 0);
			cairo_stroke(cairo);
		} break;
		case AUTH_STATE_VALIDATING: {
			cairo_set_source_rgba(cairo, 0, 114.0 / 255, 255.0 / 255, 0.75);
			cairo_fill_preserve(cairo);
			cairo_set_source_rgb(cairo, 51.0 / 255, 0, 250.0 / 255);
			cairo_stroke(cairo);
		} break;
		case AUTH_STATE_INVALID: {
			cairo_set_source_rgba(cairo, 250.0 / 255, 0, 0, 0.75);
			cairo_fill_preserve(cairo);
			cairo_set_source_rgb(cairo, 125.0 / 255, 51.0 / 255, 0);
			cairo_stroke(cairo);
		} break;
		default: break;
		}

		// Draw a message
		char *text = NULL;
		cairo_set_source_rgb(cairo, 0, 0, 0);
		cairo_select_font_face(cairo, "sans-serif",
				CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(cairo, ARC_RADIUS / 3.0f);
		switch (state->auth_state) {
		case AUTH_STATE_VALIDATING:
			text = "verifying";
			break;
		case AUTH_STATE_INVALID:
			text = "wrong";
			break;
		default: break;
		}

		if (text) {
			cairo_text_extents_t extents;
			double x, y;
			cairo_text_extents(cairo, text, &extents);
			x = (surface->width / 2) -
				(extents.width / 2 + extents.x_bearing);
			y = (surface->height / 2) -
				(extents.height / 2 + extents.y_bearing);

			cairo_move_to(cairo, x, y);
			cairo_show_text(cairo, text);
			cairo_close_path(cairo);
			cairo_new_sub_path(cairo);
		}

		// Typing indicator: Highlight random part on keypress
		if (state->auth_state == AUTH_STATE_INPUT
				|| state->auth_state == AUTH_STATE_BACKSPACE) {
			static double highlight_start = 0;
			highlight_start +=
				(rand() % (int)(M_PI * 100)) / 100.0 + M_PI * 0.5;
			cairo_arc(cairo, surface->width / 2, surface->height / 2,
					ARC_RADIUS, highlight_start,
					highlight_start + TYPE_INDICATOR_RANGE);
			if (state->auth_state == AUTH_STATE_INPUT) {
				cairo_set_source_rgb(cairo, 51.0 / 255, 219.0 / 255, 0);
			} else {
				cairo_set_source_rgb(cairo, 219.0 / 255, 51.0 / 255, 0);
			}
			cairo_stroke(cairo);

			// Draw borders
			cairo_set_source_rgb(cairo, 0, 0, 0);
			cairo_arc(cairo, surface->width / 2, surface->height / 2,
					ARC_RADIUS, highlight_start,
					highlight_start + TYPE_INDICATOR_BORDER_THICKNESS);
			cairo_stroke(cairo);

			cairo_arc(cairo, surface->width / 2, surface->height / 2,
					ARC_RADIUS, highlight_start + TYPE_INDICATOR_RANGE,
					highlight_start + TYPE_INDICATOR_RANGE +
						TYPE_INDICATOR_BORDER_THICKNESS);
			cairo_stroke(cairo);
		}

		// Draw inner + outer border of the circle
		cairo_set_source_rgb(cairo, 0, 0, 0);
		cairo_set_line_width(cairo, 2.0);
		cairo_arc(cairo, surface->width / 2, surface->height / 2,
				ARC_RADIUS - ARC_THICKNESS / 2, 0, 2 * M_PI);
		cairo_stroke(cairo);
		cairo_arc(cairo, surface->width / 2, surface->height / 2,
				ARC_RADIUS + ARC_THICKNESS / 2, 0, 2 * M_PI);
		cairo_stroke(cairo);
	}

	wl_surface_attach(surface->surface, surface->current_buffer->buffer, 0, 0);
	wl_surface_damage(surface->surface, 0, 0, surface->width, surface->height);
	wl_surface_commit(surface->surface);
	wl_display_roundtrip(state->display);
}

void render_frames(struct swaylock_state *state) {
	struct swaylock_surface *surface;
	wl_list_for_each(surface, &state->surfaces, link) {
		render_frame(surface);
	}
}
