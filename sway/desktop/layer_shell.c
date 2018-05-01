#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server.h>
#include <wlr/types/wlr_box.h>
#include <wlr/types/wlr_layer_shell.h>
#include <wlr/types/wlr_output_damage.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/layout.h"
#include "log.h"

static void apply_exclusive(struct wlr_box *usable_area,
		uint32_t anchor, int32_t exclusive,
		int32_t margin_top, int32_t margin_right,
		int32_t margin_bottom, int32_t margin_left) {
	if (exclusive <= 0) {
		return;
	}
	struct {
		uint32_t anchors;
		int *positive_axis;
		int *negative_axis;
		int margin;
	} edges[] = {
		{
			.anchors =
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP,
			.positive_axis = &usable_area->y,
			.negative_axis = &usable_area->height,
			.margin = margin_top,
		},
		{
			.anchors =
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
			.positive_axis = NULL,
			.negative_axis = &usable_area->height,
			.margin = margin_bottom,
		},
		{
			.anchors =
				ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
			.positive_axis = &usable_area->x,
			.negative_axis = &usable_area->width,
			.margin = margin_left,
		},
		{
			.anchors =
				ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
				ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM,
			.positive_axis = NULL,
			.negative_axis = &usable_area->width,
			.margin = margin_right,
		},
	};
	for (size_t i = 0; i < sizeof(edges) / sizeof(edges[0]); ++i) {
		if ((anchor & edges[i].anchors) == edges[i].anchors) {
			if (edges[i].positive_axis) {
				*edges[i].positive_axis += exclusive + edges[i].margin;
			}
			if (edges[i].negative_axis) {
				*edges[i].negative_axis -= exclusive + edges[i].margin;
			}
		}
	}
}

static void arrange_layer(struct sway_output *output, struct wl_list *list,
		struct wlr_box *usable_area, bool exclusive) {
	struct sway_layer_surface *sway_layer;
	struct wlr_box full_area = { 0 };
	wlr_output_effective_resolution(output->wlr_output,
			&full_area.width, &full_area.height);
	wl_list_for_each(sway_layer, list, link) {
		struct wlr_layer_surface *layer = sway_layer->layer_surface;
		struct wlr_layer_surface_state *state = &layer->current;
		if (exclusive != (state->exclusive_zone > 0)) {
			continue;
		}
		struct wlr_box bounds;
		if (state->exclusive_zone == -1) {
			bounds = full_area;
		} else {
			bounds = *usable_area;
		}
		struct wlr_box box = {
			.width = state->desired_width,
			.height = state->desired_height
		};
		// Horizontal axis
		const uint32_t both_horiz = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
		if ((state->anchor & both_horiz) && box.width == 0) {
			box.x = bounds.x;
			box.width = bounds.width;
		} else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)) {
			box.x = bounds.x;
		} else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
			box.x = bounds.x + (bounds.width - box.width);
		} else {
			box.x = bounds.x + ((bounds.width / 2) - (box.width / 2));
		}
		// Vertical axis
		const uint32_t both_vert = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
			| ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
		if ((state->anchor & both_vert) && box.height == 0) {
			box.y = bounds.y;
			box.height = bounds.height;
		} else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)) {
			box.y = bounds.y;
		} else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) {
			box.y = bounds.y + (bounds.height - box.height);
		} else {
			box.y = bounds.y + ((bounds.height / 2) - (box.height / 2));
		}
		// Margin
		if ((state->anchor & both_horiz) == both_horiz) {
			box.x += state->margin.left;
			box.width -= state->margin.left + state->margin.right;
		} else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)) {
			box.x += state->margin.left;
		} else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)) {
			box.x -= state->margin.right;
		}
		if ((state->anchor & both_vert) == both_vert) {
			box.y += state->margin.top;
			box.height -= state->margin.top + state->margin.bottom;
		} else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)) {
			box.y += state->margin.top;
		} else if ((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)) {
			box.y -= state->margin.bottom;
		}
		if (box.width < 0 || box.height < 0) {
			// TODO: Bubble up a protocol error?
			wlr_layer_surface_close(layer);
			continue;
		}
		// Apply
		sway_layer->geo = box;
		apply_exclusive(usable_area, state->anchor, state->exclusive_zone,
				state->margin.top, state->margin.right,
				state->margin.bottom, state->margin.left);
		wlr_layer_surface_configure(layer, box.width, box.height);
	}
}

void arrange_layers(struct sway_output *output) {
	struct wlr_box usable_area = { 0 };
	wlr_output_effective_resolution(output->wlr_output,
			&usable_area.width, &usable_area.height);

	// Arrange exclusive surfaces from top->bottom
	arrange_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],
			&usable_area, true);
	arrange_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP],
			&usable_area, true);
	arrange_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM],
			&usable_area, true);
	arrange_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND],
			&usable_area, true);

	if (memcmp(&usable_area, &output->usable_area,
				sizeof(struct wlr_box)) != 0) {
		wlr_log(L_DEBUG, "Usable area changed, rearranging output");
		memcpy(&output->usable_area, &usable_area, sizeof(struct wlr_box));
		arrange_output(output->swayc);
	}

	// Arrange non-exlusive surfaces from top->bottom
	usable_area.x = usable_area.y = 0;
	wlr_output_effective_resolution(output->wlr_output,
			&usable_area.width, &usable_area.height);
	arrange_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY],
			&usable_area, false);
	arrange_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP],
			&usable_area, false);
	arrange_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM],
			&usable_area, false);
	arrange_layer(output, &output->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND],
			&usable_area, false);

	// Find topmost keyboard interactive layer, if such a layer exists
	uint32_t layers_above_shell[] = {
		ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
		ZWLR_LAYER_SHELL_V1_LAYER_TOP,
	};
	size_t nlayers = sizeof(layers_above_shell) / sizeof(layers_above_shell[0]);
	struct sway_layer_surface *layer, *topmost = NULL;
	for (size_t i = 0; i < nlayers; ++i) {
		wl_list_for_each_reverse(layer,
				&output->layers[layers_above_shell[i]], link) {
			if (layer->layer_surface->current.keyboard_interactive) {
				topmost = layer;
				break;
			}
		}
		if (topmost != NULL) {
			break;
		}
	}

	struct sway_seat *seat;
	wl_list_for_each(seat, &input_manager->seats, link) {
		seat_set_focus_layer(seat, topmost ? topmost->layer_surface : NULL);
	}
}

static void handle_output_destroy(struct wl_listener *listener, void *data) {
	struct sway_layer_surface *sway_layer =
		wl_container_of(listener, sway_layer, output_destroy);
	wl_list_remove(&sway_layer->output_destroy.link);
	sway_layer->layer_surface->output = NULL;
	wlr_layer_surface_close(sway_layer->layer_surface);
}

static void handle_surface_commit(struct wl_listener *listener, void *data) {
	struct sway_layer_surface *layer =
		wl_container_of(listener, layer, surface_commit);
	struct wlr_layer_surface *layer_surface = layer->layer_surface;
	struct wlr_output *wlr_output = layer_surface->output;
	if (wlr_output == NULL) {
		return;
	}

	struct sway_output *output = wlr_output->data;
	struct wlr_box old_geo = layer->geo;
	arrange_layers(output);
	if (memcmp(&old_geo, &layer->geo, sizeof(struct wlr_box)) != 0) {
		output_damage_surface(output, old_geo.x, old_geo.y,
			layer_surface->surface, true);
		output_damage_surface(output, layer->geo.x, layer->geo.y,
			layer_surface->surface, true);
	} else {
		output_damage_surface(output, layer->geo.x, layer->geo.y,
			layer_surface->surface, false);
	}
}

static void unmap(struct sway_layer_surface *sway_layer) {
	struct wlr_output *wlr_output = sway_layer->layer_surface->output;
	if (wlr_output == NULL) {
		return;
	}
	struct sway_output *output = wlr_output->data;
	if (output == NULL) {
		return;
	}
	output_damage_surface(output, sway_layer->geo.x, sway_layer->geo.y,
		sway_layer->layer_surface->surface, true);
}

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_layer_surface *sway_layer =
		wl_container_of(listener, sway_layer, destroy);
	wlr_log(L_DEBUG, "Layer surface destroyed (%s)",
		sway_layer->layer_surface->namespace);
	if (sway_layer->layer_surface->mapped) {
		unmap(sway_layer);
	}
	wl_list_remove(&sway_layer->link);
	wl_list_remove(&sway_layer->destroy.link);
	wl_list_remove(&sway_layer->map.link);
	wl_list_remove(&sway_layer->unmap.link);
	wl_list_remove(&sway_layer->surface_commit.link);
	if (sway_layer->layer_surface->output != NULL) {
		struct sway_output *output = sway_layer->layer_surface->output->data;
		if (output != NULL) {
			arrange_layers(output);
		}
		wl_list_remove(&sway_layer->output_destroy.link);
		sway_layer->layer_surface->output = NULL;
	}
	free(sway_layer);
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct sway_layer_surface *sway_layer = wl_container_of(listener,
			sway_layer, map);
	struct sway_output *output = sway_layer->layer_surface->output->data;
	output_damage_surface(output, sway_layer->geo.x, sway_layer->geo.y,
		sway_layer->layer_surface->surface, true);
	// TODO: send enter to subsurfaces and popups
	wlr_surface_send_enter(sway_layer->layer_surface->surface,
		sway_layer->layer_surface->output);
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_layer_surface *sway_layer = wl_container_of(
			listener, sway_layer, unmap);
	unmap(sway_layer);
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

	if (!layer_surface->output) {
		// Assign last active output
		struct sway_container *output = NULL;
		struct sway_seat *seat = input_manager_get_default_seat(input_manager);
		if (seat) {
			output = seat_get_focus_inactive(seat, &root_container);
		}
		if (!output) {
			if (!sway_assert(root_container.children->length,
						"cannot auto-assign output for layer")) {
				wlr_layer_surface_close(layer_surface);
				return;
			}
			output = root_container.children->items[0];
		}
		if (output->type != C_OUTPUT) {
			output = container_parent(output, C_OUTPUT);
		}
		layer_surface->output = output->sway_output->wlr_output;
	}

	sway_layer->surface_commit.notify = handle_surface_commit;
	wl_signal_add(&layer_surface->surface->events.commit,
		&sway_layer->surface_commit);

	sway_layer->output_destroy.notify = handle_output_destroy;
	wl_signal_add(&layer_surface->output->events.destroy,
		&sway_layer->output_destroy);

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
