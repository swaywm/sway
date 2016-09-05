#include <xkbcommon/xkbcommon.h>
#include <stdlib.h>
#include <stdbool.h>
#include <libinput.h>
#include <math.h>
#include <wlc/wlc.h>
#include <wlc/wlc-render.h>
#include <wlc/wlc-wayland.h>
#include <ctype.h>
#include "sway/handlers.h"
#include "sway/border.h"
#include "sway/layout.h"
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/workspace.h"
#include "sway/container.h"
#include "sway/output.h"
#include "sway/focus.h"
#include "sway/input_state.h"
#include "sway/resize.h"
#include "sway/extensions.h"
#include "sway/criteria.h"
#include "sway/ipc-server.h"
#include "sway/input.h"
#include "list.h"
#include "stringop.h"
#include "log.h"

// Event should be sent to client
#define EVENT_PASSTHROUGH false

// Event handled by sway and should not be sent to client
#define EVENT_HANDLED true

static struct panel_config *if_panel_find_config(struct wl_client *client) {
	int i;
	for (i = 0; i < desktop_shell.panels->length; i++) {
		struct panel_config *config = desktop_shell.panels->items[i];
		if (config->client == client) {
			return config;
		}
	}
	return NULL;
}

static struct background_config *if_background_find_config(struct wl_client *client) {
	int i;
	for (i = 0; i < desktop_shell.backgrounds->length; i++) {
		struct background_config *config = desktop_shell.backgrounds->items[i];
		if (config->client == client) {
			return config;
		}
	}
	return NULL;
}

static struct wlc_geometry compute_panel_geometry(struct panel_config *config) {
	struct wlc_size resolution;
	output_get_scaled_size(config->output, &resolution);
	const struct wlc_geometry *old = wlc_view_get_geometry(config->handle);
	struct wlc_geometry new;

	switch (config->panel_position) {
	case DESKTOP_SHELL_PANEL_POSITION_TOP:
		new.origin.x = 0;
		new.origin.y = 0;
		new.size.w = resolution.w;
		new.size.h = old->size.h;
		break;
	case DESKTOP_SHELL_PANEL_POSITION_BOTTOM:
		new.origin.x = 0;
		new.origin.y = resolution.h - old->size.h;
		new.size.w = resolution.w;
		new.size.h = old->size.h;
		break;
	case DESKTOP_SHELL_PANEL_POSITION_LEFT:
		new.origin.x = 0;
		new.origin.y = 0;
		new.size.w = old->size.w;
		new.size.h = resolution.h;
		break;
	case DESKTOP_SHELL_PANEL_POSITION_RIGHT:
		new.origin.x = resolution.w - old->size.w;
		new.origin.y = 0;
		new.size.w = old->size.w;
		new.size.h = resolution.h;
		break;
	}

	return new;
}

static void update_panel_geometry(struct panel_config *config) {
	struct wlc_geometry geometry = compute_panel_geometry(config);
	wlc_view_set_geometry(config->handle, 0, &geometry);
}

static void update_panel_geometries(wlc_handle output) {
	for (int i = 0; i < desktop_shell.panels->length; i++) {
		struct panel_config *config = desktop_shell.panels->items[i];
		if (config->output == output) {
			update_panel_geometry(config);
		}
	}
}

static void update_background_geometry(struct background_config *config) {
	struct wlc_geometry geometry = wlc_geometry_zero;
	output_get_scaled_size(config->output, &geometry.size);
	wlc_view_set_geometry(config->handle, 0, &geometry);
}

static void update_background_geometries(wlc_handle output) {
	for (int i = 0; i < desktop_shell.backgrounds->length; i++) {
		struct background_config *config = desktop_shell.backgrounds->items[i];
		if (config->output == output) {
			update_background_geometry(config);
		}
	}
}

/* Handles */

static bool handle_input_created(struct libinput_device *device) {
	const char *identifier = libinput_dev_unique_id(device);
	sway_log(L_INFO, "Found input device (%s)", identifier);

	list_add(input_devices, device);

	struct input_config *ic = NULL;
	int i;
	for (i = 0; i < config->input_configs->length; ++i) {
		struct input_config *cur = config->input_configs->items[i];
		if (strcasecmp(identifier, cur->identifier) == 0) {
			sway_log(L_DEBUG, "Matched input config for %s",
					identifier);
			ic = cur;
			break;
		}
		if (strcasecmp("*", cur->identifier) == 0) {
			sway_log(L_DEBUG, "Matched wildcard input config for %s",
					identifier);
			ic = cur;
			break;
		}
	}

	apply_input_config(ic, device);
	return true;
}

static void handle_input_destroyed(struct libinput_device *device) {
	int i;
	list_t *list = input_devices;
	for (i = 0; i < list->length; ++i) {
		if(((struct libinput_device *)list->items[i]) == device) {
			list_del(list, i);
			break;
		}
	}
}

static bool handle_output_created(wlc_handle output) {
	swayc_t *op = new_output(output);

	// Visibility mask to be able to make view invisible
	wlc_output_set_mask(output, VISIBLE);

	if (!op) {
		return false;
	}

	// Switch to workspace if we need to
	if (swayc_active_workspace() == NULL) {
		swayc_t *ws = op->children->items[0];
		workspace_switch(ws);
	}

	// Fixes issues with backgrounds and wlc
	wlc_handle prev = wlc_get_focused_output();
	wlc_output_focus(output);
	wlc_output_focus(prev);
	return true;
}

static void handle_output_destroyed(wlc_handle output) {
	int i;
	list_t *list = root_container.children;
	for (i = 0; i < list->length; ++i) {
		if (((swayc_t *)list->items[i])->handle == output) {
			break;
		}
	}
	if (i < list->length) {
		destroy_output(list->items[i]);
	} else {
		return;
	}
	if (list->length > 0) {
		// switch to other outputs active workspace
		workspace_switch(((swayc_t *)root_container.children->items[0])->focused);
	}
}

static void handle_output_post_render(wlc_handle output) {
	ipc_get_pixels(output);
}

static void handle_view_pre_render(wlc_handle view) {
	render_view_borders(view);
}

static void handle_output_resolution_change(wlc_handle output, const struct wlc_size *from, const struct wlc_size *to) {
	sway_log(L_DEBUG, "Output %u resolution changed to %d x %d", (unsigned int)output, to->w, to->h);

	swayc_t *c = swayc_by_handle(output);
	if (!c) {
		return;
	}
	c->width = to->w;
	c->height = to->h;

	update_panel_geometries(output);
	update_background_geometries(output);

	arrange_windows(&root_container, -1, -1);
}

static void handle_output_focused(wlc_handle output, bool focus) {
	swayc_t *c = swayc_by_handle(output);
	// if for some reason this output doesn't exist, create it.
	if (!c) {
		handle_output_created(output);
	}
	if (focus) {
		set_focused_container(get_focused_container(c));
	}
}

static void ws_cleanup() {
	swayc_t *op, *ws;
	int i = 0, j;
	if (!root_container.children)
		return;
	while (i < root_container.children->length) {
		op = root_container.children->items[i++];
		if (!op->children)
			continue;
		j = 0;
		while (j < op->children->length) {
			ws = op->children->items[j++];
			if (ws->children->length == 0 && ws->floating->length == 0 && ws != op->focused) {
				if (destroy_workspace(ws)) {
					j--;
				}
			}
		}
	}
}

static bool handle_view_created(wlc_handle handle) {
	// if view is child of another view, the use that as focused container
	wlc_handle parent = wlc_view_get_parent(handle);
	swayc_t *focused = NULL;
	swayc_t *newview = NULL;
	swayc_t *current_ws = swayc_active_workspace();
	bool return_to_workspace = false;
	struct wl_client *client = wlc_view_get_wl_client(handle);
	pid_t pid;
	struct panel_config *panel_config = NULL;
	struct background_config *background_config = NULL;

	panel_config = if_panel_find_config(client);
	if (panel_config) {
		panel_config->handle = handle;
		update_panel_geometry(panel_config);
		wlc_view_set_mask(handle, VISIBLE);
		wlc_view_set_output(handle, panel_config->output);
		wlc_view_bring_to_front(handle);
		arrange_windows(&root_container, -1, -1);
		return true;
	}

	background_config = if_background_find_config(client);
	if (background_config) {
		background_config->handle = handle;
		update_background_geometry(background_config);
		wlc_view_set_mask(handle, VISIBLE);
		wlc_view_set_output(handle, background_config->output);
		wlc_view_send_to_back(handle);
		return true;
	}

	// Get parent container, to add view in
	if (parent) {
		focused = swayc_by_handle(parent);
	}

	if (client) {
		pid = wlc_view_get_pid(handle);

		if (pid) {
			// using newview as a temp storage location here,
			// rather than adding yet another workspace var
			newview = workspace_for_pid(pid);
			if (newview) {
				focused = get_focused_container(newview);
				return_to_workspace = true;
			}
			newview = NULL;
		}
	}

	if (!focused || focused->type == C_OUTPUT) {
		focused = get_focused_container(&root_container);
		// Move focus from floating view
		if (focused->is_floating) {
			// To workspace if there are no children
			if (focused->parent->children->length == 0) {
				focused = focused->parent;
			}
			// TODO find a better way of doing this
			// Or to focused container
			else {
				focused = get_focused_container(focused->parent->children->items[0]);
			}
		}
	}
	sway_log(L_DEBUG, "handle:%" PRIuPTR " type:%x state:%x parent:%" PRIuPTR " "
			"mask:%d (x:%d y:%d w:%d h:%d) title:%s "
			"class:%s appid:%s",
		handle, wlc_view_get_type(handle), wlc_view_get_state(handle), parent,
		wlc_view_get_mask(handle), wlc_view_get_geometry(handle)->origin.x,
		wlc_view_get_geometry(handle)->origin.y,wlc_view_get_geometry(handle)->size.w,
		wlc_view_get_geometry(handle)->size.h, wlc_view_get_title(handle),
		wlc_view_get_class(handle), wlc_view_get_app_id(handle));

	// TODO properly figure out how each window should be handled.
	switch (wlc_view_get_type(handle)) {
	// regular view created regularly
	case 0:
		if (parent) {
			newview = new_floating_view(handle);
		} else {
			newview = new_view(focused, handle);
			wlc_view_set_state(handle, WLC_BIT_MAXIMIZED, true);
		}
		break;

	// Dmenu keeps viewfocus, but others with this flag don't, for now simulate
	// dmenu
	case WLC_BIT_OVERRIDE_REDIRECT:
		wlc_view_focus(handle);
		wlc_view_set_state(handle, WLC_BIT_ACTIVATED, true);
		wlc_view_bring_to_front(handle);
		break;

	// Firefox popups have this flag set.
	case WLC_BIT_OVERRIDE_REDIRECT|WLC_BIT_UNMANAGED:
		wlc_view_bring_to_front(handle);
		locked_container_focus = true;
		break;

	// Modals, get focus, popups do not
	case WLC_BIT_MODAL:
		wlc_view_focus(handle);
		wlc_view_bring_to_front(handle);
		newview = new_floating_view(handle);
	case WLC_BIT_POPUP:
		wlc_view_bring_to_front(handle);
		break;
	}

	// Prevent current ws from being destroyed, if empty
	suspend_workspace_cleanup = true;

	if (newview) {
		ipc_event_window(newview, "new");
		set_focused_container(newview);
		swayc_t *output = swayc_parent_by_type(newview, C_OUTPUT);
		arrange_windows(output, -1, -1);
		// check if it matches for_window in config and execute if so
		list_t *criteria = criteria_for(newview);
		for (int i = 0; i < criteria->length; i++) {
			struct criteria *crit = criteria->items[i];
			sway_log(L_DEBUG, "for_window '%s' matches new view %p, cmd: '%s'",
					crit->crit_raw, newview, crit->cmdlist);
			struct cmd_results *res = handle_command(crit->cmdlist);
			if (res->status != CMD_SUCCESS) {
				sway_log(L_ERROR, "Command '%s' failed: %s", res->input, res->error);
			}
			free_cmd_results(res);
			// view must be focused for commands to affect it, so always
			// refocus in-between command lists
			set_focused_container(newview);
		}
		swayc_t *workspace = swayc_parent_by_type(focused, C_WORKSPACE);
		if (workspace && workspace->fullscreen) {
			set_focused_container(workspace->fullscreen);
		}

		// if parent container is a workspace, newview its only child and
		// layout is tabbed/stacked, add a container around newview
		swayc_t *parent_container = newview->parent;
		if (parent_container->type == C_WORKSPACE && parent_container->children->length == 1 &&
				(parent_container->layout == L_TABBED || parent_container->layout == L_STACKED)) {
			swayc_t *container = new_container(newview, parent_container->layout);
			set_focused_container(newview);
			arrange_windows(container, -1, -1);
		}
	} else {
		swayc_t *output = swayc_parent_by_type(focused, C_OUTPUT);
		wlc_handle *h = malloc(sizeof(wlc_handle));
		*h = handle;
		sway_log(L_DEBUG, "Adding unmanaged window %p to %p", h, output->unmanaged);
		list_add(output->unmanaged, h);
	}
	wlc_view_set_mask(handle, VISIBLE);

	if (return_to_workspace && current_ws != swayc_active_workspace()) {
		// we were on one workspace, switched to another to add this view,
		// now let's return to where we were
		workspace_switch(current_ws);
		set_focused_container(get_focused_container(current_ws));
	}

	suspend_workspace_cleanup = false;
	ws_cleanup();
	return true;
}

static void handle_view_destroyed(wlc_handle handle) {
	sway_log(L_DEBUG, "Destroying window %" PRIuPTR, handle);
	swayc_t *view = swayc_by_handle(handle);

	// destroy views by type
	switch (wlc_view_get_type(handle)) {
	// regular view created regularly
	case 0:
	case WLC_BIT_MODAL:
	case WLC_BIT_POPUP:
		break;
	// DMENU has this flag, and takes view_focus, but other things with this
	// flag don't
	case WLC_BIT_OVERRIDE_REDIRECT:
		break;
	case WLC_BIT_OVERRIDE_REDIRECT|WLC_BIT_UNMANAGED:
		locked_container_focus = false;
		break;
	}

	if (view) {
		bool fullscreen = swayc_is_fullscreen(view);
		remove_view_from_scratchpad(view);
		swayc_t *parent = destroy_view(view);
		if (fullscreen) {
			parent->fullscreen = NULL;
		}

		// Destroy empty workspaces
		if (parent->type == C_WORKSPACE &&
			parent->children->length == 0 &&
			parent->floating->length == 0 &&
			swayc_active_workspace() != parent &&
			!parent->visible) {
			parent = destroy_workspace(parent);
		}

		arrange_windows(parent, -1, -1);
		ipc_event_window(parent, "close");
	} else {
		// Is it unmanaged?
		int i;
		for (i = 0; i < root_container.children->length; ++i) {
			swayc_t *output = root_container.children->items[i];
			int j;
			for (j = 0; j < output->unmanaged->length; ++j) {
				wlc_handle *_handle = output->unmanaged->items[j];
				if (*_handle == handle) {
					list_del(output->unmanaged, j);
					free(_handle);
					break;
				}
			}
		}
	}
	set_focused_container(get_focused_view(&root_container));
}

static void handle_view_focus(wlc_handle view, bool focus) {
	return;
}

static void handle_view_geometry_request(wlc_handle handle, const struct wlc_geometry *geometry) {
	sway_log(L_DEBUG, "geometry request for %" PRIuPTR " %dx%d @ %d,%d", handle,
			geometry->size.w, geometry->size.h, geometry->origin.x, geometry->origin.y);
	// If the view is floating, then apply the geometry.
	// Otherwise save the desired width/height for the view.
	// This will not do anything for the time being as WLC improperly sends geometry requests
	swayc_t *view = swayc_by_handle(handle);
	if (view) {
		view->desired_width = geometry->size.w;
		view->desired_height = geometry->size.h;

		if (view->is_floating) {
			floating_view_sane_size(view);
			view->width = view->desired_width;
			view->height = view->desired_height;
			view->x = geometry->origin.x;
			view->y = geometry->origin.y;
			update_geometry(view);
		}
	}
}

static void handle_view_state_request(wlc_handle view, enum wlc_view_state_bit state, bool toggle) {
	swayc_t *c = swayc_by_handle(view);
	switch (state) {
	case WLC_BIT_FULLSCREEN:
		// i3 just lets it become fullscreen
		wlc_view_set_state(view, state, toggle);
		if (c) {
			sway_log(L_DEBUG, "setting view %" PRIuPTR " %s, fullscreen %d", view, c->name, toggle);
			arrange_windows(c->parent, -1, -1);
			// Set it as focused window for that workspace if its going fullscreen
			if (toggle) {
				swayc_t *ws = swayc_parent_by_type(c, C_WORKSPACE);
				// Set ws focus to c
				set_focused_container_for(ws, c);
			}
		}
		break;
	case WLC_BIT_MAXIMIZED:
	case WLC_BIT_RESIZING:
	case WLC_BIT_MOVING:
		break;
	case WLC_BIT_ACTIVATED:
		sway_log(L_DEBUG, "View %p requested to be activated", c);
		break;
	}
	return;
}

static void handle_view_properties_updated(wlc_handle view, uint32_t mask) {
	if (mask == WLC_BIT_PROPERTY_TITLE) {
		swayc_t *c = swayc_by_handle(view);
		if (!c) {
			return;
		}

		// update window title
		const char *new_name = wlc_view_get_title(view);

		if (new_name) {
			if (!c->name || strcmp(c->name, new_name) != 0) {
				free(c->name);
				c->name = strdup(new_name);
				swayc_t *p = swayc_tabbed_stacked_ancestor(c);
				if (p) {
					// TODO: we only got the topmost tabbed/stacked container, update borders of all containers on the path
					update_container_border(get_focused_view(p));
				} else if (c->border_type == B_NORMAL) {
					update_container_border(c);
				}
				ipc_event_window(c, "title");
			}
		}
	}
}

static void handle_binding_command(struct sway_binding *binding) {
	struct sway_binding *binding_copy = binding;
	bool reload = false;
	// if this is a reload command we need to make a duplicate of the
	// binding since it will be gone after the reload has completed.
	if (strcasecmp(binding->command, "reload") == 0) {
		binding_copy = sway_binding_dup(binding);
		reload = true;
	}

	struct cmd_results *res = handle_command(binding->command);
	if (res->status != CMD_SUCCESS) {
		sway_log(L_ERROR, "Command '%s' failed: %s", res->input, res->error);
	}
	ipc_event_binding_keyboard(binding_copy);

	if (reload) { // free the binding if we made a copy
		free_sway_binding(binding_copy);
	}

	free_cmd_results(res);
}

static bool handle_bindsym(struct sway_binding *binding, uint32_t keysym, uint32_t keycode) {
	int i;
	for (i = 0; i < binding->keys->length; ++i) {
		if (binding->bindcode) {
			xkb_keycode_t *key = binding->keys->items[i];
			if (keycode == *key) {
				handle_binding_command(binding);
				return true;
			}
		} else {
			xkb_keysym_t *key = binding->keys->items[i];
			if (keysym == *key) {
				handle_binding_command(binding);
				return true;
			}
		}
	}

	return false;
}

static bool valid_bindsym(struct sway_binding *binding) {
	bool match = false;
	int i;
	for (i = 0; i < binding->keys->length; ++i) {
		if (binding->bindcode) {
			xkb_keycode_t *key = binding->keys->items[i];
			if ((match = check_key(0, *key)) == false) {
				break;
			}
		} else {
			xkb_keysym_t *key = binding->keys->items[i];
			if ((match = check_key(*key, 0)) == false) {
				break;
			}
		}
	}

	return match;
}

static bool handle_bindsym_release(struct sway_binding *binding) {
	if (binding->keys->length == 1) {
		xkb_keysym_t *key = binding->keys->items[0];
		if (check_released_key(*key)) {
			handle_binding_command(binding);
			return true;
		}
	}

	return false;
}

static bool handle_key(wlc_handle view, uint32_t time, const struct wlc_modifiers *modifiers,
		uint32_t key, enum wlc_key_state state) {

	if (desktop_shell.is_locked) {
		return EVENT_PASSTHROUGH;
	}

	// reset pointer mode on keypress
	if (state == WLC_KEY_STATE_PRESSED && pointer_state.mode) {
		pointer_mode_reset();
	}

	struct sway_mode *mode = config->current_mode;

	struct wlc_modifiers no_mods = { 0, 0 };
	uint32_t sym = tolower(wlc_keyboard_get_keysym_for_key(key, &no_mods));

	int i;

	if (state == WLC_KEY_STATE_PRESSED) {
		press_key(sym, key);
	} else { // WLC_KEY_STATE_RELEASED
		release_key(sym, key);
	}

	// handle bar modifiers pressed/released
	uint32_t modifier;
	for (i = 0; i < config->active_bar_modifiers->length; ++i) {
		modifier = *(uint32_t *)config->active_bar_modifiers->items[i];

		switch (modifier_state_changed(modifiers->mods, modifier)) {
		case MOD_STATE_PRESSED:
			ipc_event_modifier(modifier, "pressed");
			break;
		case MOD_STATE_RELEASED:
			ipc_event_modifier(modifier, "released");
			break;
		}
	}
	// update modifiers state
	modifiers_state_update(modifiers->mods);

	// handle bindings
	list_t *candidates = create_list();
	for (i = 0; i < mode->bindings->length; ++i) {
		struct sway_binding *binding = mode->bindings->items[i];
		if ((modifiers->mods ^ binding->modifiers) == 0) {
			switch (state) {
			case WLC_KEY_STATE_PRESSED: {
				if (!binding->release && valid_bindsym(binding)) {
					list_add(candidates, binding);
				}
			}
			case WLC_KEY_STATE_RELEASED:
				if (binding->release && handle_bindsym_release(binding)) {
					list_free(candidates);
					return EVENT_HANDLED;
				}
				break;
			}
		}
	}

	for (i = 0; i < candidates->length; ++i) {
		struct sway_binding *binding = candidates->items[i];
		if (state == WLC_KEY_STATE_PRESSED) {
			if (!binding->release && handle_bindsym(binding, sym, key)) {
				list_free(candidates);
				return EVENT_HANDLED;
			}
		}
	}

	list_free(candidates);
	return EVENT_PASSTHROUGH;
}

static bool handle_pointer_motion(wlc_handle handle, uint32_t time, const struct wlc_point *origin) {
	if (desktop_shell.is_locked) {
		return EVENT_PASSTHROUGH;
	}

	struct wlc_point new_origin = *origin;
	// Switch to adjacent output if touching output edge.
	//
	// Since this doesn't currently support moving windows between outputs we
	// don't do the switch if the pointer is in a mode.
	if (config->seamless_mouse && !pointer_state.mode &&
			!pointer_state.left.held && !pointer_state.right.held && !pointer_state.scroll.held) {

		swayc_t *output = swayc_active_output(), *adjacent = NULL;
		struct wlc_point abs_pos = *origin;
		abs_pos.x += output->x;
		abs_pos.y += output->y;
		if (origin->x == 0) { // Left edge
			if ((adjacent = swayc_adjacent_output(output, MOVE_LEFT, &abs_pos, false))) {
				if (workspace_switch(swayc_active_workspace_for(adjacent))) {
					new_origin.x = adjacent->width;
					// adjust for differently aligned outputs (well, this is
					// only correct when the two outputs have the same
					// resolution or the same dpi I guess, it should take
					// physical attributes into account)
					new_origin.y += (output->y - adjacent->y);
				}
			}
		} else if ((double)origin->x == output->width) { // Right edge
			if ((adjacent = swayc_adjacent_output(output, MOVE_RIGHT, &abs_pos, false))) {
				if (workspace_switch(swayc_active_workspace_for(adjacent))) {
					new_origin.x = 0;
					new_origin.y += (output->y - adjacent->y);
				}
			}
		} else if (origin->y == 0) { // Top edge
			if ((adjacent = swayc_adjacent_output(output, MOVE_UP, &abs_pos, false))) {
				if (workspace_switch(swayc_active_workspace_for(adjacent))) {
					new_origin.y = adjacent->height;
					new_origin.x += (output->x - adjacent->x);
				}
			}
		} else if ((double)origin->y == output->height) { // Bottom edge
			if ((adjacent = swayc_adjacent_output(output, MOVE_DOWN, &abs_pos, false))) {
				if (workspace_switch(swayc_active_workspace_for(adjacent))) {
					new_origin.y = 0;
					new_origin.x += (output->x - adjacent->x);
				}
			}
		}
	}

	pointer_position_set(&new_origin, false);
	return EVENT_PASSTHROUGH;
}


static bool handle_pointer_button(wlc_handle view, uint32_t time, const struct wlc_modifiers *modifiers,
		uint32_t button, enum wlc_button_state state, const struct wlc_point *origin) {

	// Update view pointer is on
	pointer_state.view = container_under_pointer();

	// Update pointer_state
	switch (button) {
	case M_LEFT_CLICK:
		if (state == WLC_BUTTON_STATE_PRESSED) {
			pointer_state.left.held = true;
			pointer_state.left.x = origin->x;
			pointer_state.left.y = origin->y;
			pointer_state.left.view = pointer_state.view;
		} else {
			pointer_state.left.held = false;
		}
		break;

	case M_RIGHT_CLICK:
		if (state == WLC_BUTTON_STATE_PRESSED) {
			pointer_state.right.held = true;
			pointer_state.right.x = origin->x;
			pointer_state.right.y = origin->y;
			pointer_state.right.view = pointer_state.view;
		} else {
			pointer_state.right.held = false;
		}
		break;

	case M_SCROLL_CLICK:
		if (state == WLC_BUTTON_STATE_PRESSED) {
			pointer_state.scroll.held = true;
			pointer_state.scroll.x = origin->x;
			pointer_state.scroll.y = origin->y;
			pointer_state.scroll.view = pointer_state.view;
		} else {
			pointer_state.scroll.held = false;
		}
		break;

		//TODO scrolling behavior
	case M_SCROLL_UP:
	case M_SCROLL_DOWN:
		break;
	}

	// get focused window and check if to change focus on mouse click
	swayc_t *focused = get_focused_container(&root_container);

	// don't change focus or mode if fullscreen
	if (swayc_is_fullscreen(focused)) {
		return EVENT_PASSTHROUGH;
	}

	// set pointer mode only if floating mod has been set
	if (config->floating_mod) {
		pointer_mode_set(button, !(modifiers->mods ^ config->floating_mod));
	}

	// Check whether to change focus
	swayc_t *pointer = pointer_state.view;
	if (pointer) {
		if (focused != pointer) {
			set_focused_container(pointer_state.view);
		}
		// Send to front if floating
		if (pointer->is_floating) {
			int i;
			for (i = 0; i < pointer->parent->floating->length; i++) {
				if (pointer->parent->floating->items[i] == pointer) {
					list_del(pointer->parent->floating, i);
					list_add(pointer->parent->floating, pointer);
					break;
				}
			}
			wlc_view_bring_to_front(pointer->handle);
		}
	}

	// Return if mode has been set
	if (pointer_state.mode) {
		return EVENT_HANDLED;
	}

	// Always send mouse release
	if (state == WLC_BUTTON_STATE_RELEASED) {
		return EVENT_PASSTHROUGH;
	}

	// Finally send click
	return EVENT_PASSTHROUGH;
}

bool handle_pointer_scroll(wlc_handle view, uint32_t time, const struct wlc_modifiers* modifiers,
		uint8_t axis_bits, double _amount[2]) {
	if (!(modifiers->mods ^ config->floating_mod)) {
		int x_amount = (int)_amount[0];
		int y_amount = (int)_amount[1];

		if (x_amount > 0 && strcmp(config->floating_scroll_up_cmd, "")) {
			handle_command(config->floating_scroll_up_cmd);
			return EVENT_HANDLED;
		} else if (x_amount < 0 && strcmp(config->floating_scroll_down_cmd, "")) {
			handle_command(config->floating_scroll_down_cmd);
			return EVENT_HANDLED;
		}

		if (y_amount > 0 && strcmp(config->floating_scroll_right_cmd, "")) {
			handle_command(config->floating_scroll_right_cmd);
			return EVENT_HANDLED;
		} else if (y_amount < 0 && strcmp(config->floating_scroll_left_cmd, "")) {
			handle_command(config->floating_scroll_left_cmd);
			return EVENT_HANDLED;
		}
	}
	return EVENT_PASSTHROUGH;
}

static void handle_wlc_ready(void) {
	sway_log(L_DEBUG, "Compositor is ready, executing cmds in queue");
	// Execute commands until there are none left
	config->active = true;
	while (config->cmd_queue->length) {
		char *line = config->cmd_queue->items[0];
		struct cmd_results *res = handle_command(line);
		if (res->status != CMD_SUCCESS) {
			sway_log(L_ERROR, "Error on line '%s': %s", line, res->error);
		}
		free_cmd_results(res);
		free(line);
		list_del(config->cmd_queue, 0);
	}
}

void register_wlc_handlers() {
	wlc_set_output_created_cb(handle_output_created);
	wlc_set_output_destroyed_cb(handle_output_destroyed);
	wlc_set_output_resolution_cb(handle_output_resolution_change);
	wlc_set_output_focus_cb(handle_output_focused);
	wlc_set_output_render_post_cb(handle_output_post_render);
	wlc_set_view_created_cb(handle_view_created);
	wlc_set_view_destroyed_cb(handle_view_destroyed);
	wlc_set_view_focus_cb(handle_view_focus);
	wlc_set_view_render_pre_cb(handle_view_pre_render);
	wlc_set_view_request_geometry_cb(handle_view_geometry_request);
	wlc_set_view_request_state_cb(handle_view_state_request);
	wlc_set_view_properties_updated_cb(handle_view_properties_updated);
	wlc_set_keyboard_key_cb(handle_key);
	wlc_set_pointer_motion_cb(handle_pointer_motion);
	wlc_set_pointer_button_cb(handle_pointer_button);
	wlc_set_pointer_scroll_cb(handle_pointer_scroll);
	wlc_set_compositor_ready_cb(handle_wlc_ready);
	wlc_set_input_created_cb(handle_input_created);
	wlc_set_input_destroyed_cb(handle_input_destroyed);
}
