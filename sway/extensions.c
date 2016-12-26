#include <stdlib.h>
#include <wlc/wlc.h>
#include <wlc/wlc-wayland.h>
#include <wlc/wlc-render.h>
#include "wayland-desktop-shell-server-protocol.h"
#include "wayland-swaylock-server-protocol.h"
#include "sway/layout.h"
#include "sway/input_state.h"
#include "sway/extensions.h"
#include "sway/security.h"
#include "sway/ipc-server.h"
#include "log.h"

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
	if (!config) {
		sway_log(L_ERROR, "Unable to create panel config");
		return NULL;
	}
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
			break;
		}
	}
	if (desktop_shell.lock_surfaces->length == 0) {
		sway_log(L_DEBUG, "Desktop shell unlocked");
		desktop_shell.is_locked = false;

		// We need to now give focus back to the focus which we internally
		// track, since when we lock sway we don't actually change our internal
		// focus tracking.
		swayc_t *focus = get_focused_container(swayc_active_workspace());
		set_focused_container(focus);
		wlc_view_focus(focus->handle);
	}
}

static void set_background(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *_output, struct wl_resource *surface) {
	pid_t pid;
	wl_client_get_credentials(client, &pid, NULL, NULL);
	if (!(get_feature_policy(pid) & FEATURE_BACKGROUND)) {
		sway_log(L_INFO, "Denying background feature to %d", pid);
		return;
	}
	wlc_handle output = wlc_handle_from_wl_output_resource(_output);
	if (!output) {
		return;
	}
	sway_log(L_DEBUG, "Setting surface %p as background for output %d", surface, (int)output);
	struct background_config *config = malloc(sizeof(struct background_config));
	if (!config) {
		sway_log(L_ERROR, "Unable to allocate background config");
		return;
	}
	config->client = client;
	config->output = output;
	config->surface = wlc_resource_from_wl_surface_resource(surface);
	config->wl_surface_res = surface;
	list_add(desktop_shell.backgrounds, config);
	wl_resource_set_destructor(surface, background_surface_destructor);
	arrange_windows(swayc_by_handle(output), -1, -1);
	wlc_output_schedule_render(config->output);
}

static void set_panel(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *_output, struct wl_resource *surface) {
	pid_t pid;
	wl_client_get_credentials(client, &pid, NULL, NULL);
	if (!(get_feature_policy(pid) & FEATURE_PANEL)) {
		sway_log(L_INFO, "Denying panel feature to %d", pid);
		return;
	}
	wlc_handle output = wlc_handle_from_wl_output_resource(_output);
	if (!output) {
		return;
	}
	sway_log(L_DEBUG, "Setting surface %p as panel for output %d (wl_resource: %p)", surface, (int)output, resource);
	struct panel_config *config = find_or_create_panel_config(resource);
	config->output = output;
	config->client = client;
	config->surface = wlc_resource_from_wl_surface_resource(surface);
	config->wl_surface_res = surface;
	wl_resource_set_destructor(surface, panel_surface_destructor);
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
	pid_t pid;
	wl_client_get_credentials(client, &pid, NULL, NULL);
	if (!(get_feature_policy(pid) & FEATURE_LOCK)) {
		sway_log(L_INFO, "Denying lock feature to %d", pid);
		return;
	}
	swayc_t *output = swayc_by_handle(wlc_handle_from_wl_output_resource(_output));
	swayc_t *view = swayc_by_handle(wlc_handle_from_wl_surface_resource(surface));
	sway_log(L_DEBUG, "Setting lock surface to %p", view);
	if (view && output) {
		swayc_t *workspace = output->focused;
		if (!swayc_is_child_of(view, workspace)) {
			move_container_to(view, workspace);
		}
		// make the view floating so it doesn't rearrange other siblings.
		if (!view->is_floating) {
			destroy_container(remove_child(view));
			add_floating(workspace, view);
		}
		wlc_view_set_state(view->handle, WLC_BIT_FULLSCREEN, true);
		wlc_view_bring_to_front(view->handle);
		wlc_view_focus(view->handle);
		desktop_shell.is_locked = true;
		input_init();
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
	pid_t pid;
	wl_client_get_credentials(client, &pid, NULL, NULL);
	if (!(get_feature_policy(pid) & FEATURE_PANEL)) {
		sway_log(L_INFO, "Denying panel feature to %d", pid);
		return;
	}
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
