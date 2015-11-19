#include <wlc/wlc.h>
#include <wlc/wlc-wayland.h>
#include "wayland-desktop-shell-server-protocol.h"
#include "log.h"
#include "extensions.h"

struct desktop_shell_state desktop_shell;

static void set_background(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *_output, struct wl_resource *surface) {
	wlc_handle output = wlc_handle_from_wl_output_resource(_output);
	if (!output) {
		return;
	}
	sway_log(L_DEBUG, "Setting surface %p as background for output %d", surface, (int)output);
	struct background_config *config = malloc(sizeof(struct background_config));
	config->output = output;
	config->surface = wlc_resource_from_wl_surface_resource(surface);
	list_add(desktop_shell.backgrounds, config);
}

static struct desktop_shell_interface desktop_shell_implementation = {
	.set_background = set_background
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
	desktop_shell.backgrounds = create_list();
}
