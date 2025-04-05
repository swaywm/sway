#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include "log.h"
#include "sway/scene_descriptor.h"
#include "sway/desktop/transaction.h"
#include "sway/input/cursor.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/workspace.h"

struct wlr_layer_surface_v1 *toplevel_layer_surface_from_surface(
		struct wlr_surface *surface) {
	struct wlr_layer_surface_v1 *layer;
	do {
		if (!surface) {
			return NULL;
		}
		// Topmost layer surface
		if ((layer = wlr_layer_surface_v1_try_from_wlr_surface(surface))) {
			return layer;
		}
		// Layer subsurface
		if (wlr_subsurface_try_from_wlr_surface(surface)) {
			surface = wlr_surface_get_root_surface(surface);
			continue;
		}

		// Layer surface popup
		struct wlr_xdg_surface *xdg_surface = NULL;
		if ((xdg_surface = wlr_xdg_surface_try_from_wlr_surface(surface)) &&
				xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP && xdg_surface->popup != NULL) {
			if (!xdg_surface->popup->parent) {
				return NULL;
			}
			surface = wlr_surface_get_root_surface(xdg_surface->popup->parent);
			continue;
		}

		// Return early if the surface is not a layer/xdg_popup/sub surface
		return NULL;
	} while (true);
}

static void arrange_surface(struct sway_output *output, const struct wlr_box *full_area,
		struct wlr_box *usable_area, struct wlr_scene_tree *tree, bool exclusive) {
	struct wlr_scene_node *node;
	wl_list_for_each(node, &tree->children, link) {
		struct sway_layer_surface *surface = scene_descriptor_try_get(node,
			SWAY_SCENE_DESC_LAYER_SHELL);
		// surface could be null during destruction
		if (!surface) {
			continue;
		}

		if (!surface->scene->layer_surface->initialized) {
			continue;
		}

		if ((surface->scene->layer_surface->current.exclusive_zone > 0) != exclusive) {
			continue;
		}

		wlr_scene_layer_surface_v1_configure(surface->scene, full_area, usable_area);
	}
}

void arrange_layers(struct sway_output *output) {
	struct wlr_box usable_area = { 0 };
	wlr_output_effective_resolution(output->wlr_output,
			&usable_area.width, &usable_area.height);
	const struct wlr_box full_area = usable_area;

	arrange_surface(output, &full_area, &usable_area, output->layers.shell_overlay, true);
	arrange_surface(output, &full_area, &usable_area, output->layers.shell_top, true);
	arrange_surface(output, &full_area, &usable_area, output->layers.shell_bottom, true);
	arrange_surface(output, &full_area, &usable_area, output->layers.shell_background, true);

	arrange_surface(output, &full_area, &usable_area, output->layers.shell_overlay, false);
	arrange_surface(output, &full_area, &usable_area, output->layers.shell_top, false);
	arrange_surface(output, &full_area, &usable_area, output->layers.shell_bottom, false);
	arrange_surface(output, &full_area, &usable_area, output->layers.shell_background, false);

	if (!wlr_box_equal(&usable_area, &output->usable_area)) {
		sway_log(SWAY_DEBUG, "Usable area changed, rearranging output");
		output->usable_area = usable_area;
		arrange_output(output);
	} else {
		arrange_popups(root->layers.popup);
	}

	// Find topmost keyboard interactive layer, if such a layer exists
	struct wlr_scene_tree *layers_above_shell[] = {
		output->layers.shell_overlay,
		output->layers.shell_top,
	};
	size_t nlayers = sizeof(layers_above_shell) / sizeof(layers_above_shell[0]);
	struct wlr_scene_node *node;
	struct sway_layer_surface *topmost = NULL;
	for (size_t i = 0; i < nlayers; ++i) {
		wl_list_for_each_reverse(node,
				&layers_above_shell[i]->children, link) {
			struct sway_layer_surface *surface = scene_descriptor_try_get(node,
				SWAY_SCENE_DESC_LAYER_SHELL);
			if (surface && surface->layer_surface->current.keyboard_interactive
					== ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE &&
					surface->layer_surface->surface->mapped) {
				topmost = surface;
				break;
			}
		}
		if (topmost != NULL) {
			break;
		}
	}

	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		seat->has_exclusive_layer = false;
		if (topmost != NULL) {
			seat_set_focus_layer(seat, topmost->layer_surface);
		} else if (seat->focused_layer &&
				seat->focused_layer->current.keyboard_interactive
					!= ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE) {
			seat_set_focus_layer(seat, NULL);
		}
	}
}

static struct wlr_scene_tree *sway_layer_get_scene(struct sway_output *output,
		enum zwlr_layer_shell_v1_layer type) {
	switch (type) {
	case ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND:
		return output->layers.shell_background;
	case ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM:
		return output->layers.shell_bottom;
	case ZWLR_LAYER_SHELL_V1_LAYER_TOP:
		return output->layers.shell_top;
	case ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY:
		return output->layers.shell_overlay;
	}

	sway_assert(false, "unreachable");
	return NULL;
}

static struct sway_layer_surface *sway_layer_surface_create(
		struct wlr_scene_layer_surface_v1 *scene) {
	struct sway_layer_surface *surface = calloc(1, sizeof(*surface));
	if (!surface) {
		sway_log(SWAY_ERROR, "Could not allocate a scene_layer surface");
		return NULL;
	}

	struct wlr_scene_tree *popups = wlr_scene_tree_create(root->layers.popup);
	if (!popups) {
		sway_log(SWAY_ERROR, "Could not allocate a scene_layer popup node");
		free(surface);
		return NULL;
	}

	surface->desc.relative = &scene->tree->node;

	if (!scene_descriptor_assign(&popups->node,
			SWAY_SCENE_DESC_POPUP, &surface->desc)) {
		sway_log(SWAY_ERROR, "Failed to allocate a popup scene descriptor");
		wlr_scene_node_destroy(&popups->node);
		free(surface);
		return NULL;
	}

	surface->tree = scene->tree;
	surface->scene = scene;
	surface->layer_surface = scene->layer_surface;
	surface->popups = popups;
	surface->layer_surface->data = surface;

	return surface;
}

static struct sway_layer_surface *find_mapped_layer_by_client(
		struct wl_client *client, struct sway_output *ignore_output) {
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		if (output == ignore_output) {
			continue;
		}
		// For now we'll only check the overlay layer
		struct wlr_scene_node *node;
		wl_list_for_each (node, &output->layers.shell_overlay->children, link) {
			struct sway_layer_surface *surface = scene_descriptor_try_get(node,
				SWAY_SCENE_DESC_LAYER_SHELL);
			if (!surface) {
				continue;
			}

			struct wlr_layer_surface_v1 *layer_surface = surface->layer_surface;
			struct wl_resource *resource = layer_surface->resource;
			if (wl_resource_get_client(resource) == client
					&& layer_surface->surface->mapped) {
				return surface;
			}
		}
	}
	return NULL;
}

static void handle_node_destroy(struct wl_listener *listener, void *data) {
	struct sway_layer_surface *layer =
		wl_container_of(listener, layer, node_destroy);

	// destroy the scene descriptor straight away if it exists, otherwise
	// we will try to reflow still considering the destroyed node.
	scene_descriptor_destroy(&layer->tree->node, SWAY_SCENE_DESC_LAYER_SHELL);

	// Determine if this layer is being used by an exclusive client. If it is,
	// try and find another layer owned by this client to pass focus to.
	struct sway_seat *seat = input_manager_get_default_seat();
	struct wl_client *client =
		wl_resource_get_client(layer->layer_surface->resource);
	if (!server.session_lock.lock) {
		struct sway_layer_surface *consider_layer =
			find_mapped_layer_by_client(client, layer->output);
		if (consider_layer) {
			seat_set_focus_layer(seat, consider_layer->layer_surface);
		}
	}

	if (layer->output) {
		arrange_layers(layer->output);
		transaction_commit_dirty();
	}

	wlr_scene_node_destroy(&layer->popups->node);

	wl_list_remove(&layer->map.link);
	wl_list_remove(&layer->unmap.link);
	wl_list_remove(&layer->surface_commit.link);
	wl_list_remove(&layer->node_destroy.link);
	wl_list_remove(&layer->new_popup.link);

	layer->layer_surface->data = NULL;

	wl_list_remove(&layer->link);
	free(layer);
}

static void handle_surface_commit(struct wl_listener *listener, void *data) {
	struct sway_layer_surface *surface =
		wl_container_of(listener, surface, surface_commit);

	struct wlr_layer_surface_v1 *layer_surface = surface->layer_surface;
	uint32_t committed = layer_surface->current.committed;
	if (layer_surface->initialized && committed & WLR_LAYER_SURFACE_V1_STATE_LAYER) {
		enum zwlr_layer_shell_v1_layer layer_type = layer_surface->current.layer;
		struct wlr_scene_tree *output_layer = sway_layer_get_scene(
			surface->output, layer_type);
		wlr_scene_node_reparent(&surface->scene->tree->node, output_layer);
	}

	if (layer_surface->initial_commit || committed || layer_surface->surface->mapped != surface->mapped) {
		surface->mapped = layer_surface->surface->mapped;
		arrange_layers(surface->output);
		transaction_commit_dirty();
	}
}

static void handle_map(struct wl_listener *listener, void *data) {
	struct sway_layer_surface *surface = wl_container_of(listener,
			surface, map);

	struct wlr_layer_surface_v1 *layer_surface =
				surface->scene->layer_surface;

	// focus on new surface
	if (layer_surface->current.keyboard_interactive &&
			(layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY ||
			layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_TOP)) {
		struct sway_seat *seat;
		wl_list_for_each(seat, &server.input->seats, link) {
			// but only if the currently focused layer has a lower precedence
			if (!seat->focused_layer ||
					seat->focused_layer->current.layer >= layer_surface->current.layer) {
				seat_set_focus_layer(seat, layer_surface);
			}
		}
		arrange_layers(surface->output);
	}

	cursor_rebase_all();
}

static void handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_layer_surface *surface = wl_container_of(
			listener, surface, unmap);
	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		if (seat->focused_layer == surface->layer_surface) {
			seat_set_focus_layer(seat, NULL);
		}
	}

	cursor_rebase_all();
}

static void popup_handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_layer_popup *popup =
		wl_container_of(listener, popup, destroy);

	wl_list_remove(&popup->destroy.link);
	wl_list_remove(&popup->new_popup.link);
	wl_list_remove(&popup->commit.link);
	free(popup);
}

static void popup_unconstrain(struct sway_layer_popup *popup) {
	struct wlr_xdg_popup *wlr_popup = popup->wlr_popup;
	struct sway_output *output = popup->toplevel->output;

	// if a client tries to create a popup while we are in the process of destroying
	// its output, don't crash.
	if (!output) {
		return;
	}

	double lx, ly;
	wlr_scene_node_coords(&popup->toplevel->scene->tree->node, &lx, &ly);

	// the output box expressed in the coordinate system of the toplevel parent
	// of the popup
	struct wlr_box output_toplevel_sx_box = {
		.x = output->lx - lx,
		.y = output->ly - ly,
		.width = output->width,
		.height = output->height,
	};

	wlr_xdg_popup_unconstrain_from_box(wlr_popup, &output_toplevel_sx_box);
}

static void popup_handle_commit(struct wl_listener *listener, void *data) {
	struct sway_layer_popup *popup = wl_container_of(listener, popup, commit);
	if (popup->wlr_popup->base->initial_commit) {
		popup_unconstrain(popup);
	}
}

static void popup_handle_new_popup(struct wl_listener *listener, void *data);

static struct sway_layer_popup *create_popup(struct wlr_xdg_popup *wlr_popup,
		struct sway_layer_surface *toplevel, struct wlr_scene_tree *parent) {
	struct sway_layer_popup *popup = calloc(1, sizeof(*popup));
	if (popup == NULL) {
		return NULL;
	}

	popup->toplevel = toplevel;
	popup->wlr_popup = wlr_popup;
	popup->scene = wlr_scene_xdg_surface_create(parent,
		wlr_popup->base);

	if (!popup->scene) {
		free(popup);
		return NULL;
	}

	popup->destroy.notify = popup_handle_destroy;
	wl_signal_add(&wlr_popup->base->events.destroy, &popup->destroy);
	popup->new_popup.notify = popup_handle_new_popup;
	wl_signal_add(&wlr_popup->base->events.new_popup, &popup->new_popup);
	popup->commit.notify = popup_handle_commit;
	wl_signal_add(&wlr_popup->base->surface->events.commit, &popup->commit);

	return popup;
}

static void popup_handle_new_popup(struct wl_listener *listener, void *data) {
	struct sway_layer_popup *sway_layer_popup =
		wl_container_of(listener, sway_layer_popup, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;
	create_popup(wlr_popup, sway_layer_popup->toplevel, sway_layer_popup->scene);
}

static void handle_new_popup(struct wl_listener *listener, void *data) {
	struct sway_layer_surface *sway_layer_surface =
		wl_container_of(listener, sway_layer_surface, new_popup);
	struct wlr_xdg_popup *wlr_popup = data;
	create_popup(wlr_popup, sway_layer_surface, sway_layer_surface->popups);
}

void handle_layer_shell_surface(struct wl_listener *listener, void *data) {
	struct wlr_layer_surface_v1 *layer_surface = data;
	sway_log(SWAY_DEBUG, "new layer surface: namespace %s layer %d anchor %" PRIu32
			" size %" PRIu32 "x%" PRIu32 " margin %" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",",
		layer_surface->namespace,
		layer_surface->pending.layer,
		layer_surface->pending.anchor,
		layer_surface->pending.desired_width,
		layer_surface->pending.desired_height,
		layer_surface->pending.margin.top,
		layer_surface->pending.margin.right,
		layer_surface->pending.margin.bottom,
		layer_surface->pending.margin.left);

	if (!layer_surface->output) {
		// Assign last active output
		struct sway_output *output = NULL;
		struct sway_seat *seat = input_manager_get_default_seat();
		if (seat) {
			struct sway_workspace *ws = seat_get_focused_workspace(seat);
			if (ws != NULL) {
				output = ws->output;
			}
		}
		if (!output || output == root->fallback_output) {
			if (!root->outputs->length) {
				sway_log(SWAY_ERROR,
						"no output to auto-assign layer surface '%s' to",
						layer_surface->namespace);
				wlr_layer_surface_v1_destroy(layer_surface);
				return;
			}
			output = root->outputs->items[0];
		}
		layer_surface->output = output->wlr_output;
	}

	struct sway_output *output = layer_surface->output->data;

	enum zwlr_layer_shell_v1_layer layer_type = layer_surface->pending.layer;
	struct wlr_scene_tree *output_layer = sway_layer_get_scene(
		output, layer_type);
	struct wlr_scene_layer_surface_v1 *scene_surface =
		wlr_scene_layer_surface_v1_create(output_layer, layer_surface);
	if (!scene_surface) {
		sway_log(SWAY_ERROR, "Could not allocate a layer_surface_v1");
		return;
	}

	struct sway_layer_surface *surface =
		sway_layer_surface_create(scene_surface);
	if (!surface) {
		wlr_layer_surface_v1_destroy(layer_surface);

		sway_log(SWAY_ERROR, "Could not allocate a sway_layer_surface");
		return;
	}

	if (!scene_descriptor_assign(&scene_surface->tree->node,
			SWAY_SCENE_DESC_LAYER_SHELL, surface)) {
		sway_log(SWAY_ERROR, "Failed to allocate a layer surface descriptor");
		// destroying the layer_surface will also destroy its corresponding
		// scene node
		wlr_layer_surface_v1_destroy(layer_surface);
		return;
	}

	surface->output = output;
	wl_list_insert(&output->layer_surfaces, &surface->link);

	// now that the surface's output is known, we can advertise its scale
	wlr_fractional_scale_v1_notify_scale(surface->layer_surface->surface,
		layer_surface->output->scale);
	wlr_surface_set_preferred_buffer_scale(surface->layer_surface->surface,
		ceil(layer_surface->output->scale));

	surface->surface_commit.notify = handle_surface_commit;
	wl_signal_add(&layer_surface->surface->events.commit,
		&surface->surface_commit);
	surface->map.notify = handle_map;
	wl_signal_add(&layer_surface->surface->events.map, &surface->map);
	surface->unmap.notify = handle_unmap;
	wl_signal_add(&layer_surface->surface->events.unmap, &surface->unmap);
	surface->new_popup.notify = handle_new_popup;
	wl_signal_add(&layer_surface->events.new_popup, &surface->new_popup);

	surface->node_destroy.notify = handle_node_destroy;
	wl_signal_add(&scene_surface->tree->node.events.destroy, &surface->node_destroy);
}

void destroy_layers(struct sway_output *output) {
	struct sway_layer_surface *layer, *layer_tmp;
	wl_list_for_each_safe(layer, layer_tmp, &output->layer_surfaces, link) {
		layer->output = NULL;
		wlr_layer_surface_v1_destroy(layer->layer_surface);
	}
}
