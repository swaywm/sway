#include "sway/lockscreen-overlay.h"
#include "sway/desktop/transaction.h"
#include "sway/layers.h"

#include "kde-lockscreen-overlay-v1-protocol.h"
#include "log.h"
#include "sway/tree/arrange.h"

#include <stdlib.h>
#include <wayland-server-core.h>

#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_layer_shell_v1.h>

struct sway_lockscreen_overlay {
};

static void kde_lockscreen_overlay_allow(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *surface) {
	sway_log(SWAY_ERROR, "at %s:%d", __func__, __LINE__);

	struct wlr_surface *overlay_surface = wlr_surface_from_resource(surface);
	// TODO: send a protocol error for each of these checks?
	if (!overlay_surface) {
		sway_log(SWAY_ERROR, "No overlay surface found");
		return;
	}

	struct wlr_layer_surface_v1 *wlr_layer_surface =
		wlr_layer_surface_v1_try_from_wlr_surface(overlay_surface);
	if (!wlr_layer_surface) {
		sway_log(SWAY_ERROR, "no wlr_layer surface found");
		return;
	}

	struct sway_layer_surface *layer_surface = wlr_layer_surface->data;
	if (!layer_surface) {
		sway_log(SWAY_ERROR, "no sway_layer surface found");
		return;
	}

	layer_surface->show_over_lockscreen = true;

	arrange_layers(layer_surface->output);
	arrange_root();
	transaction_commit_dirty();
}

static void kde_lockscreen_overlay_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct kde_lockscreen_overlay_v1_interface impl = {
	.allow = kde_lockscreen_overlay_allow,
	.destroy = kde_lockscreen_overlay_destroy,
};

static void lockscreen_overlay_bind(
	struct wl_client *client,
	void *data,
	uint32_t version,
	uint32_t id
) {
	struct sway_lockscreen_overlay *overlay = data;

	struct wl_resource *resource = wl_resource_create(client,
		&kde_lockscreen_overlay_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &impl, overlay, NULL);
}

struct sway_lockscreen_overlay *sway_lockscreen_overlay_create(struct wl_display *display) {
	struct sway_lockscreen_overlay *overlay = calloc(1, sizeof(*overlay));

	wl_global_create(
		display,
		&kde_lockscreen_overlay_v1_interface, 1,
		overlay, lockscreen_overlay_bind
	);

	return overlay;
}
