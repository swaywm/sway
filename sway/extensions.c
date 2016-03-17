#include <stdlib.h>
#include <wlc/wlc.h>
#include <wlc/wlc-wayland.h>
#include <wlc/wlc-render.h>
#include "wayland-desktop-shell-server-protocol.h"
#include "wayland-swaylock-server-protocol.h"
#include "layout.h"
#include "log.h"
#include "input_state.h"
#include "extensions.h"

struct desktop_shell_state desktop_shell;

static struct panel_config *find_or_create_panel_config(struct wl_resource *resource) {
	for (int i = 0; i < desktop_shell.panels->length; i++) {
		struct panel_config *conf = desktop_shell.panels->items[i];
		if (conf->wl_resource == resource) {
			sway_log(L_DEBUG, "Found existing panel config for resource %p", resource);
			return conf;
		}
	}
	sway_log(L_DEBUG, "Creating panel config for resource %p", resource);
	struct panel_config *config = calloc(1, sizeof(struct panel_config));
	list_add(desktop_shell.panels, config);
	config->wl_resource = resource;
	return config;
}

void background_surface_destructor(struct wl_resource *resource) {
	sway_log(L_DEBUG, "Background surface killed");
	int i;
	for (i = 0; i < desktop_shell.backgrounds->length; ++i) {
		struct background_config *config = desktop_shell.backgrounds->items[i];
		if (config->wl_surface_res == resource) {
			list_del(desktop_shell.backgrounds, i);
			break;
		}
	}
}

void panel_surface_destructor(struct wl_resource *resource) {
	sway_log(L_DEBUG, "Panel surface killed");
	int i;
	for (i = 0; i < desktop_shell.panels->length; ++i) {
		struct panel_config *config = desktop_shell.panels->items[i];
		if (config->wl_surface_res == resource) {
			list_del(desktop_shell.panels, i);
			arrange_windows(&root_container, -1, -1);
			break;
		}
	}
}

void lock_surface_destructor(struct wl_resource *resource) {
	sway_log(L_DEBUG, "Lock surface killed");
	int i;
	for (i = 0; i < desktop_shell.lock_surfaces->length; ++i) {
		struct wl_resource *surface = desktop_shell.lock_surfaces->items[i];
		if (surface == resource) {
			list_del(desktop_shell.lock_surfaces, i);
			arrange_windows(&root_container, -1, -1);
			desktop_shell.is_locked = false;
			break;
		}
	}
}

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
	config->wl_surface_res = surface;
	list_add(desktop_shell.backgrounds, config);
	wl_resource_set_destructor(surface, background_surface_destructor);
	wlc_output_schedule_render(config->output);
}

static void set_panel(struct wl_client *client, struct wl_resource *resource,
			  struct wl_resource *_output, struct wl_resource *surface) {
	wlc_handle output = wlc_handle_from_wl_output_resource(_output);
	if (!output) {
		return;
	}
	sway_log(L_DEBUG, "Setting surface %p as panel for output %d (wl_resource: %p)", surface, (int)output, resource);
	struct panel_config *config = find_or_create_panel_config(resource);
	config->output = output;
	config->surface = wlc_resource_from_wl_surface_resource(surface);
	config->wl_surface_res = surface;
	wl_resource_set_destructor(surface, panel_surface_destructor);
	desktop_shell.panel_size = *wlc_surface_get_size(config->surface);
	arrange_windows(&root_container, -1, -1);
	wlc_output_schedule_render(config->output);
}

static void desktop_set_lock_surface(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surface) {
	sway_log(L_ERROR, "desktop_set_lock_surface is not currently supported");
}

static void desktop_unlock(struct wl_client *client, struct wl_resource *resource) {
	sway_log(L_ERROR, "desktop_unlock is not currently supported");
}

static void set_lock_surface(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *_output, struct wl_resource *surface) {
	swayc_t *output = swayc_by_handle(wlc_handle_from_wl_output_resource(_output));
	swayc_t *view = swayc_by_handle(wlc_handle_from_wl_surface_resource(surface));
	sway_log(L_DEBUG, "Setting lock surface to %p", view);
	if (view && output) {
		swayc_t *workspace = output->focused;
		if (!swayc_is_child_of(view, workspace)) {
			move_container_to(view, workspace);
		}
		// make the view floating so it doesn't rearrange other
		// siblings.
		if (!view->is_floating) {
			// Remove view from its current location
			destroy_container(remove_child(view));
			// and move it into workspace floating
			add_floating(workspace, view);
		}
		wlc_view_set_state(view->handle, WLC_BIT_FULLSCREEN, true);
		workspace->fullscreen = view;
		desktop_shell.is_locked = true;
		// reset input state
		input_init();
		set_focused_container(view);
		arrange_windows(workspace, -1, -1);
		list_add(desktop_shell.lock_surfaces, surface);
		wl_resource_set_destructor(surface, lock_surface_destructor);
	} else {
		sway_log(L_ERROR, "Attempted to set lock surface to non-view");
	}
}

static void unlock(struct wl_client *client, struct wl_resource *resource) {
	sway_log(L_ERROR, "unlock is not currently supported");
	// This isn't really necessary, we just unlock when the client exits.
}

static void set_grab_surface(struct wl_client *client, struct wl_resource *resource, struct wl_resource *surface) {
	sway_log(L_ERROR, "desktop_set_grab_surface is not currently supported");
}

static void desktop_ready(struct wl_client *client, struct wl_resource *resource) {
	// nop
}

static void set_panel_position(struct wl_client *client, struct wl_resource *resource, uint32_t position) {
	struct panel_config *config = find_or_create_panel_config(resource);
	sway_log(L_DEBUG, "Panel position for wl_resource %p changed %d => %d", resource, config->panel_position, position);
	config->panel_position = position;
	arrange_windows(&root_container, -1, -1);
}

static struct desktop_shell_interface desktop_shell_implementation = {
	.set_background = set_background,
	.set_panel = set_panel,
	.set_lock_surface = desktop_set_lock_surface,
	.unlock = desktop_unlock,
	.set_grab_surface = set_grab_surface,
	.desktop_ready = desktop_ready,
	.set_panel_position = set_panel_position
};

static struct lock_interface swaylock_implementation = {
	.set_lock_surface = set_lock_surface,
	.unlock = unlock
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

static void swaylock_bind(struct wl_client *client, void *data,
		unsigned int version, unsigned int id) {
	if (version > 1) {
		// Unsupported version
		return;
	}

	struct wl_resource *resource = wl_resource_create(client, &lock_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
	}

	wl_resource_set_implementation(resource, &swaylock_implementation, NULL, NULL);
}

void register_extensions(void) {
	wl_global_create(wlc_get_wl_display(), &desktop_shell_interface, 3, NULL, desktop_shell_bind);
	desktop_shell.backgrounds = create_list();
	desktop_shell.panels = create_list();
	desktop_shell.lock_surfaces = create_list();
	desktop_shell.is_locked = false;
	wl_global_create(wlc_get_wl_display(), &lock_interface, 1, NULL, swaylock_bind);
}
