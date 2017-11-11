#include <wayland-server.h>
#include <wlr/types/wlr_output.h>
#include "sway/server.h"
#include "sway/container.h"
#include "sway/workspace.h"
#include "log.h"

void output_add_notify(struct wl_listener *listener, void *data) {
	struct sway_server *server = wl_container_of(listener, server, output_add);
	struct wlr_output *wlr_output = data;
	sway_log(L_DEBUG, "New output %p: %s", wlr_output, wlr_output->name);
	swayc_t *op = new_output(wlr_output);
	if (!sway_assert(op, "Failed to allocate output")) {
		return;
	}
	// Switch to workspace if we need to
	if (swayc_active_workspace() == NULL) {
		swayc_t *ws = op->children->items[0];
		workspace_switch(ws);
	}
}
