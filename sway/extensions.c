#include <wlc/wlc.h>
#include <wlc/wlc-wayland.h>
#include "wayland-desktop-shell-server-protocol.h"
#include "log.h"

static void set_background(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *output, struct wl_resource *surface) {
	sway_log(L_DEBUG, "Surface requesting background for output");
}

static struct desktop_shell_interface desktop_shell_implementation = {
	.set_background = set_background,
};

static void desktop_shell_bind(struct wl_client *client, void *data,
		unsigned int version, unsigned int id) {
	if (version > 1) {
		// Unsupported version
		return;
	}

	struct wl_resource *resource = wl_resource_create(client, &desktop_shell_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
	}

	wl_resource_set_implementation(resource, &desktop_shell_implementation, NULL, NULL);
}

void register_extensions(void) {
	wl_global_create(wlc_get_wl_display(), &desktop_shell_interface, 1, NULL, desktop_shell_bind);
}
