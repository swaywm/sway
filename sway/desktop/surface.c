#define _POSIX_C_SOURCE 200112L
#include <stdlib.h>
#include <time.h>
#include <wlr/types/wlr_surface.h>
#include "sway/server.h"
#include "sway/surface.h"

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
