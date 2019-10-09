#include <stdlib.h>
#include <wlr/types/wlr_surface.h>
#include "sway/server.h"
#include "sway/surface.h"

void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_surface *surface = wl_container_of(listener, surface, destroy);

	surface->wlr_surface->data = NULL;
	wl_list_remove(&surface->destroy.link);

	free(surface);
}

void handle_compositor_new_surface(struct wl_listener *listener, void *data) {
	struct wlr_surface *wlr_surface = data;

	struct sway_surface *surface = calloc(1, sizeof(struct sway_surface));
	surface->wlr_surface = wlr_surface;
	wlr_surface->data = surface;

	surface->destroy.notify = handle_destroy;
	wl_signal_add(&wlr_surface->events.destroy, &surface->destroy);
}
