#include <stdlib.h>
#include <wayland-client.h>
#include "background-image.h"
#include "cairo.h"
#include "swaybg.h"

void render_surface(struct swaybg_surface *surface) {
	struct swaybg_state *state = surface->state;

	int buffer_width = surface->width * surface->scale;
	int buffer_height = surface->height * surface->scale;
	if (buffer_width == 0 || buffer_height == 0) {
		return; // not yet configured
	}

	surface->current_buffer = get_next_buffer(state->shm, surface->buffers,
		buffer_width, buffer_height);
	if (surface->current_buffer == NULL) {
		return;
	}

	cairo_t *cairo = surface->current_buffer->cairo;
	cairo_identity_matrix(cairo);

	cairo_save(cairo);
	cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
	if (state->args.mode == BACKGROUND_MODE_SOLID_COLOR || !surface->image) {
		cairo_set_source_u32(cairo, state->args.color);
		cairo_paint(cairo);
	} else {
		render_background_image(cairo, surface->image,
				state->args.mode, buffer_width, buffer_height);
	}
	cairo_restore(cairo);

	wl_surface_set_buffer_scale(surface->surface, surface->scale);
	wl_surface_attach(surface->surface, surface->current_buffer->buffer, 0, 0);
	wl_surface_damage(surface->surface, 0, 0, surface->width, surface->height);
	wl_surface_commit(surface->surface);
}
