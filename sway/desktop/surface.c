#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <time.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include "sway/server.h"
#include "sway/surface.h"
#include "sway/output.h"

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_surface *surface = wl_container_of(listener, surface, destroy);

	surface->wlr_surface->data = NULL;
	wl_list_remove(&surface->destroy.link);

	if (surface->frame_done_timer) {
		wl_event_source_remove(surface->frame_done_timer);
	}

	free(surface);
}

static int surface_frame_done_timer_handler(void *data) {
	struct sway_surface *surface = data;

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_surface_send_frame_done(surface->wlr_surface, &now);

	return 0;
}

void handle_compositor_new_surface(struct wl_listener *listener, void *data) {
	struct wlr_surface *wlr_surface = data;

	struct sway_surface *surface = calloc(1, sizeof(struct sway_surface));
	surface->wlr_surface = wlr_surface;
	wlr_surface->data = surface;

	surface->destroy.notify = handle_destroy;
	wl_signal_add(&wlr_surface->events.destroy, &surface->destroy);

	surface->frame_done_timer = wl_event_loop_add_timer(server.wl_event_loop,
		surface_frame_done_timer_handler, surface);
	if (!surface->frame_done_timer) {
		wl_resource_post_no_memory(wlr_surface->resource);
	}
}

void surface_update_outputs(struct wlr_surface *surface) {
	float scale = 1;
	struct wlr_surface_output *surface_output;
	wl_list_for_each(surface_output, &surface->current_outputs, link) {
		if (surface_output->output->scale > scale) {
			scale = surface_output->output->scale;
		}
	}
	wlr_fractional_scale_v1_notify_scale(surface, scale);
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
