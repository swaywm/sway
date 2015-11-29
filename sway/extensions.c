#include <stdlib.h>
#include <wlc/wlc.h>
#include <wlc/wlc-wayland.h>
#include "wayland-desktop-shell-server-protocol.h"
#include "layout.h"
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

static void set_panel(struct wl_client *client, struct wl_resource *resource,
			  struct wl_resource *_output, struct wl_resource *surface) {
	wlc_handle output = wlc_handle_from_wl_output_resource(_output);
	if (!output) {
		return;
	}
	sway_log(L_DEBUG, "Setting surface %p as panel for output %d", surface, (int)output);
	struct panel_config *config = malloc(sizeof(struct panel_config));
	config->output = output;
	config->surface = wlc_resource_from_wl_surface_resource(surface);
	list_add(desktop_shell.panels, config);
	desktop_shell.panel_size = *wlc_surface_get_size(config->surface);
	arrange_windows(&root_container, -1, -1);
}

static void set_lock_surface(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surface) {
	sway_log(L_ERROR, "desktop_set_lock_surface is not currently supported");
}

static void unlock(struct wl_client *client, struct wl_resource *resource) {
	sway_log(L_ERROR, "desktop_unlock is not currently supported");
}

static void set_grab_surface(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surface) {
	sway_log(L_ERROR, "desktop_set_grab_surface is not currently supported");
}

static void desktop_ready(struct wl_client *client, struct wl_resource *resource) {
	// nop
}

static void set_panel_position(struct wl_client *client, struct wl_resource *resource, uint32_t position) {
	desktop_shell.panel_position = position;
	arrange_windows(&root_container, -1, -1);
}

static struct desktop_shell_interface desktop_shell_implementation = {
	.set_background = set_background,
	.set_panel = set_panel,
	.set_lock_surface = set_lock_surface,
	.unlock = unlock,
	.set_grab_surface = set_grab_surface,
	.desktop_ready = desktop_ready,
	.set_panel_position = set_panel_position
};

static void desktop_shell_bind(struct wl_client *client, void *data,
		unsigned int version, unsigned int id) {
	if (version > 3) {
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
	wl_global_create(wlc_get_wl_display(), &desktop_shell_interface, 3, NULL, desktop_shell_bind);
	desktop_shell.backgrounds = create_list();
	desktop_shell.panels = create_list();
	desktop_shell.panel_position = DESKTOP_SHELL_PANEL_POSITION_BOTTOM;
}
