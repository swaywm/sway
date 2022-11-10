#include <stdlib.h>
#include "log.h"
#include "sway/scene_descriptor.h"

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_scene_descriptor *desc =
		wl_container_of(listener, desc, destroy);

	desc->node->data = NULL;
	wl_list_remove(&desc->destroy.link);

	free(desc);
}

void scene_descriptor_assign(struct wlr_scene_node *node,
		enum sway_scene_descriptor_type type, void *data) {
	struct sway_scene_descriptor *desc =
		calloc(1, sizeof(struct sway_scene_descriptor));

	if (!desc) {
		sway_log(SWAY_ERROR, "Could not allocate a scene descriptor");
		return;
	}

	desc->type = type;
	desc->data = data;
	desc->node = node;

	desc->destroy.notify = handle_destroy;
	wl_signal_add(&node->events.destroy, &desc->destroy);

	node->data = desc;
}
