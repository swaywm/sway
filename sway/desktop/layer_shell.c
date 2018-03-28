#include <stdlib.h>
#include <wayland-server.h>
#include <wlr/util/log.h>
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/server.h"

static void arrange_layers(struct sway_output *output) {
	// TODO
}

static void handle_output_destroy(struct wl_listener *listener, void *data) {
	// TODO
}

static void handle_output_mode(struct wl_listener *listener, void *data) {
	// TODO
}

static void handle_output_transform(struct wl_listener *listener, void *data) {
	// TODO
}

static void handle_surface_commit(struct wl_listener *listener, void *data) {
	// TODO
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	// TODO
}

static void handle_map(struct wl_listener *listener, void *data) {
	// TODO
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	// TODO
}

void handle_layer_shell_surface(struct wl_listener *listener, void *data) {
	struct wlr_layer_surface *layer_surface = data;
	struct sway_server *server =
		wl_container_of(listener, server, layer_shell_surface);
	wlr_log(L_DEBUG, "new layer surface: namespace %s layer %d anchor %d "
			"size %dx%d margin %d,%d,%d,%d",
		layer_surface->namespace, layer_surface->layer, layer_surface->layer,
		layer_surface->client_pending.desired_width,
		layer_surface->client_pending.desired_height,
		layer_surface->client_pending.margin.top,
		layer_surface->client_pending.margin.right,
		layer_surface->client_pending.margin.bottom,
		layer_surface->client_pending.margin.left);

	struct sway_layer_surface *sway_layer =
		calloc(1, sizeof(struct sway_layer_surface));
	if (!sway_layer) {
		return;
	}

	sway_layer->surface_commit.notify = handle_surface_commit;
	wl_signal_add(&layer_surface->surface->events.commit,
		&sway_layer->surface_commit);

	sway_layer->output_destroy.notify = handle_output_destroy;
	wl_signal_add(&layer_surface->output->events.destroy,
		&sway_layer->output_destroy);

	sway_layer->output_mode.notify = handle_output_mode;
	wl_signal_add(&layer_surface->output->events.mode,
		&sway_layer->output_mode);

	sway_layer->output_transform.notify = handle_output_transform;
	wl_signal_add(&layer_surface->output->events.transform,
		&sway_layer->output_transform);

	sway_layer->destroy.notify = handle_destroy;
	wl_signal_add(&layer_surface->events.destroy, &sway_layer->destroy);
	sway_layer->map.notify = handle_map;
	wl_signal_add(&layer_surface->events.map, &sway_layer->map);
	sway_layer->unmap.notify = handle_unmap;
	wl_signal_add(&layer_surface->events.unmap, &sway_layer->unmap);
	// TODO: Listen for subsurfaces

	sway_layer->layer_surface = layer_surface;
	layer_surface->data = sway_layer;

	struct sway_output *output = layer_surface->output->data;
	wl_list_insert(&output->layers[layer_surface->layer], &sway_layer->link);

	// Temporarily set the layer's current state to client_pending
	// So that we can easily arrange it
	struct wlr_layer_surface_state old_state = layer_surface->current;
	layer_surface->current = layer_surface->client_pending;
	arrange_layers(output);
	layer_surface->current = old_state;
}
