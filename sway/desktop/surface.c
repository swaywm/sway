#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include "sway/server.h"
#include "sway/surface.h"
#include "sway/output.h"

static void surface_update_outputs(struct wlr_surface *surface) {
	float scale = 1;
	struct wlr_surface_output *surface_output;
	wl_list_for_each(surface_output, &surface->current_outputs, link) {
		if (surface_output->output->scale > scale) {
			scale = surface_output->output->scale;
		}
	}
	wlr_fractional_scale_v1_notify_scale(surface, scale);
	wlr_surface_set_preferred_buffer_scale(surface, ceil(scale));
}

void surface_enter_output(struct wlr_surface *surface,
		struct sway_output *output) {
	wlr_surface_send_enter(surface, output->wlr_output);
	surface_update_outputs(surface);
}

void surface_leave_output(struct wlr_surface *surface,
		struct sway_output *output) {
	wlr_surface_send_leave(surface, output->wlr_output);
	surface_update_outputs(surface);
}
