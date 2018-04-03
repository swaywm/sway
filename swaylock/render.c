#include <wayland-client.h>
#include "cairo.h"
#include "background-image.h"
#include "swaylock/swaylock.h"

void render_frame(struct swaylock_surface *surface) {
	struct swaylock_state *state = surface->state;
	surface->current_buffer = get_next_buffer(state->shm,
			surface->buffers, surface->width, surface->height);
	cairo_t *cairo = surface->current_buffer->cairo;
	if (state->args.mode == BACKGROUND_MODE_SOLID_COLOR) {
		cairo_set_source_u32(cairo, state->args.color);
		cairo_paint(cairo);
	} else {
		render_background_image(cairo, surface->image,
				state->args.mode, surface->width, surface->height);
	}
	wl_surface_attach(surface->surface, surface->current_buffer->buffer, 0, 0);
	wl_surface_damage(surface->surface, 0, 0, surface->width, surface->height);
	wl_surface_commit(surface->surface);
}
