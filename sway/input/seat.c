#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 199309L
#include <assert.h>
#include <errno.h>
#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif
#include <strings.h>
#include <time.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include "log.h"
#include "config.h"
#include "sway/debug.h"
#include "sway/desktop.h"
#include "sway/input/cursor.h"
#include "sway/input/input-manager.h"
#include "sway/input/keyboard.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

static void seat_device_destroy(struct sway_seat_device *seat_device) {
	if (!seat_device) {
		return;
	}

	sway_keyboard_destroy(seat_device->keyboard);
	wlr_cursor_detach_input_device(seat_device->sway_seat->cursor->cursor,
		seat_device->input_device->wlr_device);
	wl_list_remove(&seat_device->link);
	free(seat_device);
}

void seat_destroy(struct sway_seat *seat) {
	struct sway_seat_device *seat_device, *next;
	wl_list_for_each_safe(seat_device, next, &seat->devices, link) {
		seat_device_destroy(seat_device);
	}
	sway_cursor_destroy(seat->cursor);
	wl_list_remove(&seat->new_node.link);
	wl_list_remove(&seat->new_drag_icon.link);
	wl_list_remove(&seat->link);
	wlr_seat_destroy(seat->wlr_seat);
}

static struct sway_seat_node *seat_node_from_node(
		struct sway_seat *seat, struct sway_node *node);

static void seat_node_destroy(struct sway_seat_node *seat_node) {
	wl_list_remove(&seat_node->destroy.link);
	wl_list_remove(&seat_node->link);
	free(seat_node);
}

/**
 * Activate all views within this container recursively.
 */
static void seat_send_activate(struct sway_node *node, struct sway_seat *seat) {
	if (node_is_view(node)) {
		if (!seat_is_input_allowed(seat, node->sway_container->view->surface)) {
			wlr_log(WLR_DEBUG, "Refusing to set focus, input is inhibited");
			return;
		}
		view_set_activated(node->sway_container->view, true);
	} else {
		list_t *children = node_get_children(node);
		for (int i = 0; i < children->length; ++i) {
			struct sway_container *child = children->items[i];
			seat_send_activate(&child->node, seat);
		}
	}
}

/**
 * If con is a view, set it as active and enable keyboard input.
 * If con is a container, set all child views as active and don't enable
 * keyboard input on any.
 */
static void seat_send_focus(struct sway_node *node, struct sway_seat *seat) {
	seat_send_activate(node, seat);

	struct sway_view *view = node->type == N_CONTAINER ?
		node->sway_container->view : NULL;

	if (view && seat_is_input_allowed(seat, view->surface)) {
#ifdef HAVE_XWAYLAND
		if (view->type == SWAY_VIEW_XWAYLAND) {
			struct wlr_xwayland *xwayland =
				seat->input->server->xwayland.wlr_xwayland;
			wlr_xwayland_set_seat(xwayland, seat->wlr_seat);
		}
#endif
		struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat->wlr_seat);
		if (keyboard) {
			wlr_seat_keyboard_notify_enter(seat->wlr_seat,
					view->surface, keyboard->keycodes,
					keyboard->num_keycodes, &keyboard->modifiers);
		} else {
			wlr_seat_keyboard_notify_enter(
					seat->wlr_seat, view->surface, NULL, 0, NULL);
		}
	}
}

void seat_for_each_node(struct sway_seat *seat,
		void (*f)(struct sway_node *node, void *data), void *data) {
	struct sway_seat_node *current = NULL;
	wl_list_for_each(current, &seat->focus_stack, link) {
		f(current->node, data);
	}
}

struct sway_container *seat_get_focus_inactive_view(struct sway_seat *seat,
		struct sway_node *ancestor) {
	if (ancestor->type == N_CONTAINER && ancestor->sway_container->view) {
		return ancestor->sway_container;
	}
	struct sway_seat_node *current;
	wl_list_for_each(current, &seat->focus_stack, link) {
		struct sway_node *node = current->node;
		if (node->type == N_CONTAINER && node->sway_container->view &&
				node_has_ancestor(node, ancestor)) {
			return node->sway_container;
		}
	}
	return NULL;
}

static void handle_seat_node_destroy(struct wl_listener *listener, void *data) {
	struct sway_seat_node *seat_node =
		wl_container_of(listener, seat_node, destroy);
	struct sway_seat *seat = seat_node->seat;
	struct sway_node *node = seat_node->node;
	struct sway_node *parent = node_get_parent(node);
	struct sway_node *focus = seat_get_focus(seat);

	bool set_focus =
		focus != NULL &&
		(focus == node || node_has_ancestor(focus, node)) &&
		node->type == N_CONTAINER;

	seat_node_destroy(seat_node);

	if (set_focus) {
		struct sway_node *next_focus = NULL;
		while (next_focus == NULL) {
			struct sway_container *con =
				seat_get_focus_inactive_view(seat, parent);
			next_focus = con ? &con->node : NULL;

			if (next_focus == NULL && parent->type == N_WORKSPACE) {
				next_focus = parent;
				break;
			}

			parent = node_get_parent(parent);
		}

		// the structure change might have caused it to move up to the top of
		// the focus stack without sending focus notifications to the view
		seat_send_focus(next_focus, seat);
		seat_set_focus(seat, next_focus);
	}
}

static struct sway_seat_node *seat_node_from_node(
		struct sway_seat *seat, struct sway_node *node) {
	if (node->type == N_ROOT || node->type == N_OUTPUT) {
		// these don't get seat nodes ever
		return NULL;
	}

	struct sway_seat_node *seat_node = NULL;
	wl_list_for_each(seat_node, &seat->focus_stack, link) {
		if (seat_node->node == node) {
			return seat_node;
		}
	}

	seat_node = calloc(1, sizeof(struct sway_seat_node));
	if (seat_node == NULL) {
		wlr_log(WLR_ERROR, "could not allocate seat node");
		return NULL;
	}

	seat_node->node = node;
	seat_node->seat = seat;
	wl_list_insert(seat->focus_stack.prev, &seat_node->link);
	wl_signal_add(&node->events.destroy, &seat_node->destroy);
	seat_node->destroy.notify = handle_seat_node_destroy;

	return seat_node;
}

static void handle_new_node(struct wl_listener *listener, void *data) {
	struct sway_seat *seat = wl_container_of(listener, seat, new_node);
	struct sway_node *node = data;
	seat_node_from_node(seat, node);
}

static void drag_icon_damage_whole(struct sway_drag_icon *icon) {
	if (!icon->wlr_drag_icon->mapped) {
		return;
	}
	desktop_damage_surface(icon->wlr_drag_icon->surface, icon->x, icon->y, true);
}

void drag_icon_update_position(struct sway_drag_icon *icon) {
	drag_icon_damage_whole(icon);

	struct wlr_drag_icon *wlr_icon = icon->wlr_drag_icon;
	struct sway_seat *seat = icon->seat;
	struct wlr_cursor *cursor = seat->cursor->cursor;
	if (wlr_icon->is_pointer) {
		icon->x = cursor->x;
		icon->y = cursor->y;
	} else {
		struct wlr_touch_point *point =
			wlr_seat_touch_get_point(seat->wlr_seat, wlr_icon->touch_id);
		if (point == NULL) {
			return;
		}
		icon->x = seat->touch_x;
		icon->y = seat->touch_y;
	}

	drag_icon_damage_whole(icon);
}

static void drag_icon_handle_surface_commit(struct wl_listener *listener,
		void *data) {
	struct sway_drag_icon *icon =
		wl_container_of(listener, icon, surface_commit);
	drag_icon_update_position(icon);
}

static void drag_icon_handle_map(struct wl_listener *listener, void *data) {
	struct sway_drag_icon *icon = wl_container_of(listener, icon, map);
	drag_icon_damage_whole(icon);
}

static void drag_icon_handle_unmap(struct wl_listener *listener, void *data) {
	struct sway_drag_icon *icon = wl_container_of(listener, icon, unmap);
	drag_icon_damage_whole(icon);
}

static void drag_icon_handle_destroy(struct wl_listener *listener, void *data) {
	struct sway_drag_icon *icon = wl_container_of(listener, icon, destroy);
	icon->wlr_drag_icon->data = NULL;
	wl_list_remove(&icon->link);
	wl_list_remove(&icon->surface_commit.link);
	wl_list_remove(&icon->unmap.link);
	wl_list_remove(&icon->destroy.link);
	free(icon);
}

static void handle_new_drag_icon(struct wl_listener *listener, void *data) {
	struct sway_seat *seat = wl_container_of(listener, seat, new_drag_icon);
	struct wlr_drag_icon *wlr_drag_icon = data;

	struct sway_drag_icon *icon = calloc(1, sizeof(struct sway_drag_icon));
	if (icon == NULL) {
		wlr_log(WLR_ERROR, "Allocation failed");
		return;
	}
	icon->seat = seat;
	icon->wlr_drag_icon = wlr_drag_icon;
	wlr_drag_icon->data = icon;

	icon->surface_commit.notify = drag_icon_handle_surface_commit;
	wl_signal_add(&wlr_drag_icon->surface->events.commit, &icon->surface_commit);
	icon->unmap.notify = drag_icon_handle_unmap;
	wl_signal_add(&wlr_drag_icon->events.unmap, &icon->unmap);
	icon->map.notify = drag_icon_handle_map;
	wl_signal_add(&wlr_drag_icon->events.map, &icon->map);
	icon->destroy.notify = drag_icon_handle_destroy;
	wl_signal_add(&wlr_drag_icon->events.destroy, &icon->destroy);

	wl_list_insert(&root->drag_icons, &icon->link);

	drag_icon_update_position(icon);
	seat_end_mouse_operation(seat);
}

static void collect_focus_iter(struct sway_node *node, void *data) {
	struct sway_seat *seat = data;
	struct sway_seat_node *seat_node = seat_node_from_node(seat, node);
	if (!seat_node) {
		return;
	}
	wl_list_remove(&seat_node->link);
	wl_list_insert(&seat->focus_stack, &seat_node->link);
}

static void collect_focus_workspace_iter(struct sway_workspace *workspace,
		void *data) {
	collect_focus_iter(&workspace->node, data);
}

static void collect_focus_container_iter(struct sway_container *container,
		void *data) {
	collect_focus_iter(&container->node, data);
}

struct sway_seat *seat_create(struct sway_input_manager *input,
		const char *seat_name) {
	struct sway_seat *seat = calloc(1, sizeof(struct sway_seat));
	if (!seat) {
		return NULL;
	}

	seat->wlr_seat = wlr_seat_create(input->server->wl_display, seat_name);
	if (!sway_assert(seat->wlr_seat, "could not allocate seat")) {
		free(seat);
		return NULL;
	}
	seat->wlr_seat->data = seat;

	seat->cursor = sway_cursor_create(seat);
	if (!seat->cursor) {
		wlr_seat_destroy(seat->wlr_seat);
		free(seat);
		return NULL;
	}

	// init the focus stack
	wl_list_init(&seat->focus_stack);

	root_for_each_workspace(collect_focus_workspace_iter, seat);
	root_for_each_container(collect_focus_container_iter, seat);

	wl_signal_add(&root->events.new_node, &seat->new_node);
	seat->new_node.notify = handle_new_node;

	wl_signal_add(&seat->wlr_seat->events.new_drag_icon, &seat->new_drag_icon);
	seat->new_drag_icon.notify = handle_new_drag_icon;

	seat->input = input;
	wl_list_init(&seat->devices);

	wlr_seat_set_capabilities(seat->wlr_seat,
		WL_SEAT_CAPABILITY_KEYBOARD |
		WL_SEAT_CAPABILITY_POINTER |
		WL_SEAT_CAPABILITY_TOUCH);


	wl_list_insert(&input->seats, &seat->link);

	return seat;
}

static void seat_apply_input_config(struct sway_seat *seat,
		struct sway_seat_device *sway_device) {
	const char *mapped_to_output = NULL;

	struct input_config *ic = input_device_get_config(
			sway_device->input_device);
	if (ic != NULL) {
		wlr_log(WLR_DEBUG, "Applying input config to %s",
			sway_device->input_device->identifier);

		mapped_to_output = ic->mapped_to_output;
	}

	if (mapped_to_output == NULL) {
		mapped_to_output = sway_device->input_device->wlr_device->output_name;
	}
	if (mapped_to_output != NULL) {
		wlr_log(WLR_DEBUG, "Mapping input device %s to output %s",
			sway_device->input_device->identifier, mapped_to_output);
		struct sway_output *output = output_by_name(mapped_to_output);
		if (output) {
			wlr_cursor_map_input_to_output(seat->cursor->cursor,
				sway_device->input_device->wlr_device, output->wlr_output);
			wlr_log(WLR_DEBUG, "Mapped to output %s", output->wlr_output->name);
		}
	}
}

static void seat_configure_pointer(struct sway_seat *seat,
		struct sway_seat_device *sway_device) {
	seat_configure_xcursor(seat);
	wlr_cursor_attach_input_device(seat->cursor->cursor,
		sway_device->input_device->wlr_device);
	seat_apply_input_config(seat, sway_device);
}

static void seat_configure_keyboard(struct sway_seat *seat,
		struct sway_seat_device *seat_device) {
	if (!seat_device->keyboard) {
		sway_keyboard_create(seat, seat_device);
	}
	struct wlr_keyboard *wlr_keyboard =
		seat_device->input_device->wlr_device->keyboard;
	sway_keyboard_configure(seat_device->keyboard);
	wlr_seat_set_keyboard(seat->wlr_seat,
			seat_device->input_device->wlr_device);
	struct sway_node *focus = seat_get_focus(seat);
	if (focus && node_is_view(focus)) {
		// force notify reenter to pick up the new configuration
		wlr_seat_keyboard_clear_focus(seat->wlr_seat);
		wlr_seat_keyboard_notify_enter(seat->wlr_seat,
				focus->sway_container->view->surface, wlr_keyboard->keycodes,
				wlr_keyboard->num_keycodes, &wlr_keyboard->modifiers);
	}
}

static void seat_configure_touch(struct sway_seat *seat,
		struct sway_seat_device *sway_device) {
	wlr_cursor_attach_input_device(seat->cursor->cursor,
		sway_device->input_device->wlr_device);
	seat_apply_input_config(seat, sway_device);
}

static void seat_configure_tablet_tool(struct sway_seat *seat,
		struct sway_seat_device *sway_device) {
	wlr_cursor_attach_input_device(seat->cursor->cursor,
		sway_device->input_device->wlr_device);
	seat_apply_input_config(seat, sway_device);
}

static struct sway_seat_device *seat_get_device(struct sway_seat *seat,
		struct sway_input_device *input_device) {
	struct sway_seat_device *seat_device = NULL;
	wl_list_for_each(seat_device, &seat->devices, link) {
		if (seat_device->input_device == input_device) {
			return seat_device;
		}
	}

	return NULL;
}

void seat_configure_device(struct sway_seat *seat,
		struct sway_input_device *input_device) {
	struct sway_seat_device *seat_device = seat_get_device(seat, input_device);
	if (!seat_device) {
		return;
	}

	switch (input_device->wlr_device->type) {
		case WLR_INPUT_DEVICE_POINTER:
			seat_configure_pointer(seat, seat_device);
			break;
		case WLR_INPUT_DEVICE_KEYBOARD:
			seat_configure_keyboard(seat, seat_device);
			break;
		case WLR_INPUT_DEVICE_TOUCH:
			seat_configure_touch(seat, seat_device);
			break;
		case WLR_INPUT_DEVICE_TABLET_TOOL:
			seat_configure_tablet_tool(seat, seat_device);
			break;
		case WLR_INPUT_DEVICE_TABLET_PAD:
			wlr_log(WLR_DEBUG, "TODO: configure tablet pad");
			break;
	}
}

void seat_add_device(struct sway_seat *seat,
		struct sway_input_device *input_device) {
	if (seat_get_device(seat, input_device)) {
		seat_configure_device(seat, input_device);
		return;
	}

	struct sway_seat_device *seat_device =
		calloc(1, sizeof(struct sway_seat_device));
	if (!seat_device) {
		wlr_log(WLR_DEBUG, "could not allocate seat device");
		return;
	}

	wlr_log(WLR_DEBUG, "adding device %s to seat %s",
		input_device->identifier, seat->wlr_seat->name);

	seat_device->sway_seat = seat;
	seat_device->input_device = input_device;
	wl_list_insert(&seat->devices, &seat_device->link);

	seat_configure_device(seat, input_device);
}

void seat_remove_device(struct sway_seat *seat,
		struct sway_input_device *input_device) {
	struct sway_seat_device *seat_device = seat_get_device(seat, input_device);

	if (!seat_device) {
		return;
	}

	wlr_log(WLR_DEBUG, "removing device %s from seat %s",
		input_device->identifier, seat->wlr_seat->name);

	seat_device_destroy(seat_device);
}

void seat_configure_xcursor(struct sway_seat *seat) {
	// TODO configure theme and size
	const char *cursor_theme = NULL;

	if (!seat->cursor->xcursor_manager) {
		seat->cursor->xcursor_manager =
			wlr_xcursor_manager_create(cursor_theme, 24);
		if (sway_assert(seat->cursor->xcursor_manager,
					"Cannot create XCursor manager for theme %s",
					cursor_theme)) {
			return;
		}
	}

	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *sway_output = root->outputs->items[i];
		struct wlr_output *output = sway_output->wlr_output;
		bool result =
			wlr_xcursor_manager_load(seat->cursor->xcursor_manager,
				output->scale);

		sway_assert(!result,
			"Cannot load xcursor theme for output '%s' with scale %f",
			// TODO: Fractional scaling
			output->name, (double)output->scale);
	}

	wlr_xcursor_manager_set_cursor_image(seat->cursor->xcursor_manager,
		"left_ptr", seat->cursor->cursor);
	wlr_cursor_warp(seat->cursor->cursor, NULL, seat->cursor->cursor->x,
		seat->cursor->cursor->y);
}

bool seat_is_input_allowed(struct sway_seat *seat,
		struct wlr_surface *surface) {
	struct wl_client *client = wl_resource_get_client(surface->resource);
	return !seat->exclusive_client || seat->exclusive_client == client;
}

static void send_unfocus(struct sway_container *con, void *data) {
	if (con->view) {
		view_set_activated(con->view, false);
	}
}

// Unfocus the container and any children (eg. when leaving `focus parent`)
static void seat_send_unfocus(struct sway_node *node, struct sway_seat *seat) {
	wlr_seat_keyboard_clear_focus(seat->wlr_seat);
	if (node->type == N_WORKSPACE) {
		workspace_for_each_container(node->sway_workspace, send_unfocus, seat);
	} else {
		send_unfocus(node->sway_container, seat);
		container_for_each_child(node->sway_container, send_unfocus, seat);
	}
}

static int handle_urgent_timeout(void *data) {
	struct sway_view *view = data;
	view_set_urgent(view, false);
	return 0;
}

void seat_set_focus_warp(struct sway_seat *seat, struct sway_node *node,
		bool warp, bool notify) {
	if (seat->focused_layer) {
		return;
	}

	struct sway_node *last_focus = seat_get_focus(seat);
	if (last_focus == node) {
		return;
	}

	struct sway_workspace *last_workspace = seat_get_focused_workspace(seat);

	if (node == NULL) {
		// Close any popups on the old focus
		if (node_is_view(last_focus)) {
			view_close_popups(last_focus->sway_container->view);
		}
		seat_send_unfocus(last_focus, seat);
		seat->has_focus = false;
		update_debug_tree();
		return;
	}

	struct sway_workspace *new_workspace = node->type == N_WORKSPACE ?
		node->sway_workspace : node->sway_container->workspace;
	struct sway_container *container = node->type == N_CONTAINER ?
		node->sway_container : NULL;

	// Deny setting focus to a view which is hidden by a fullscreen container
	if (new_workspace && new_workspace->fullscreen && container &&
			!container_is_fullscreen_or_child(container)) {
		return;
	}

	struct sway_output *last_output = last_workspace ?
		last_workspace->output : NULL;
	struct sway_output *new_output = new_workspace->output;

	if (last_workspace != new_workspace && new_output) {
		node_set_dirty(&new_output->node);
	}

	// find new output's old workspace, which might have to be removed if empty
	struct sway_workspace *new_output_last_ws = NULL;
	if (new_output && last_output != new_output) {
		new_output_last_ws = output_get_active_workspace(new_output);
	}

	// Unfocus the previous focus
	if (last_focus) {
		seat_send_unfocus(last_focus, seat);
		node_set_dirty(last_focus);
		struct sway_node *parent = node_get_parent(last_focus);
		if (parent) {
			node_set_dirty(parent);
		}
	}

	// Put the container parents on the focus stack, then the workspace, then
	// the focused container.
	if (container) {
		struct sway_container *parent = container->parent;
		while (parent) {
			struct sway_seat_node *seat_node =
				seat_node_from_node(seat, &parent->node);
			wl_list_remove(&seat_node->link);
			wl_list_insert(&seat->focus_stack, &seat_node->link);
			node_set_dirty(&parent->node);
			parent = parent->parent;
		}
	}
	if (new_workspace) {
		struct sway_seat_node *seat_node =
			seat_node_from_node(seat, &new_workspace->node);
		wl_list_remove(&seat_node->link);
		wl_list_insert(&seat->focus_stack, &seat_node->link);
		node_set_dirty(&new_workspace->node);
	}
	if (container) {
		struct sway_seat_node *seat_node =
			seat_node_from_node(seat, &container->node);
		wl_list_remove(&seat_node->link);
		wl_list_insert(&seat->focus_stack, &seat_node->link);
		node_set_dirty(&container->node);
		seat_send_focus(&container->node, seat);
	}

	// emit ipc events
	if (notify && new_workspace && last_workspace != new_workspace) {
		 ipc_event_workspace(last_workspace, new_workspace, "focus");
	}
	if (container && container->view) {
		ipc_event_window(container, "focus");
	}

	if (new_output_last_ws) {
		workspace_consider_destroy(new_output_last_ws);
	}

	// Close any popups on the old focus
	if (last_focus && node_is_view(last_focus)) {
		view_close_popups(last_focus->sway_container->view);
	}

	// If urgent, either unset the urgency or start a timer to unset it
	if (container && container->view && view_is_urgent(container->view) &&
			!container->view->urgent_timer) {
		struct sway_view *view = container->view;
		if (last_workspace && last_workspace != new_workspace &&
				config->urgent_timeout > 0) {
			view->urgent_timer = wl_event_loop_add_timer(server.wl_event_loop,
					handle_urgent_timeout, view);
			if (view->urgent_timer) {
				wl_event_source_timer_update(view->urgent_timer,
						config->urgent_timeout);
			} else {
				wlr_log(WLR_ERROR, "Unable to create urgency timer (%s)",
						strerror(errno));
				handle_urgent_timeout(view);
			}
		} else {
			view_set_urgent(view, false);
		}
	}

	// If we've focused a floating container, bring it to the front.
	// We do this by putting it at the end of the floating list.
	if (container) {
		struct sway_container *floater = container;
		while (floater->parent) {
			floater = floater->parent;
		}
		if (container_is_floating(floater)) {
			list_move_to_end(floater->workspace->floating, floater);
			node_set_dirty(&floater->workspace->node);
		}
	}

	if (last_focus) {
		if (last_workspace) {
			workspace_consider_destroy(last_workspace);
		}

		if (config->mouse_warping && warp && new_output != last_output) {
			double x = 0;
			double y = 0;
			if (container) {
				x = container->x + container->width / 2.0;
				y = container->y + container->height / 2.0;
			} else {
				x = new_workspace->x + new_workspace->width / 2.0;
				y = new_workspace->y + new_workspace->height / 2.0;
			}
			if (!wlr_output_layout_contains_point(root->output_layout,
					new_output->wlr_output, seat->cursor->cursor->x,
					seat->cursor->cursor->y)) {
				wlr_cursor_warp(seat->cursor->cursor, NULL, x, y);
				cursor_send_pointer_motion(seat->cursor, 0, true);
			}
		}
	}

	seat->has_focus = true;

	update_debug_tree();
}

void seat_set_focus(struct sway_seat *seat, struct sway_node *node) {
	seat_set_focus_warp(seat, node, true, true);
}

void seat_set_focus_container(struct sway_seat *seat,
		struct sway_container *con) {
	seat_set_focus_warp(seat, con ? &con->node : NULL, true, true);
}

void seat_set_focus_workspace(struct sway_seat *seat,
		struct sway_workspace *ws) {
	seat_set_focus_warp(seat, ws ? &ws->node : NULL, true, true);
}

void seat_set_focus_surface(struct sway_seat *seat,
		struct wlr_surface *surface, bool unfocus) {
	if (seat->focused_layer != NULL) {
		return;
	}
	if (seat->has_focus && unfocus) {
		struct sway_node *focus = seat_get_focus(seat);
		seat_send_unfocus(focus, seat);
		seat->has_focus = false;
	}
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat->wlr_seat);
	if (keyboard) {
		wlr_seat_keyboard_notify_enter(seat->wlr_seat, surface,
			keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
	} else {
		wlr_seat_keyboard_notify_enter(seat->wlr_seat, surface, NULL, 0, NULL);
	}
}

void seat_set_focus_layer(struct sway_seat *seat,
		struct wlr_layer_surface *layer) {
	if (!layer && seat->focused_layer) {
		seat->focused_layer = NULL;
		struct sway_node *previous = seat_get_focus_inactive(seat, &root->node);
		if (previous) {
			// Hack to get seat to re-focus the return value of get_focus
			seat_set_focus(seat, NULL);
			seat_set_focus(seat, previous);
		}
		return;
	} else if (!layer || seat->focused_layer == layer) {
		return;
	}
	seat_set_focus_surface(seat, layer->surface, true);
	if (layer->layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP) {
		seat->focused_layer = layer;
	}
}

void seat_set_exclusive_client(struct sway_seat *seat,
		struct wl_client *client) {
	if (!client) {
		seat->exclusive_client = client;
		// Triggers a refocus of the topmost surface layer if necessary
		// TODO: Make layer surface focus per-output based on cursor position
		for (int i = 0; i < root->outputs->length; ++i) {
			struct sway_output *output = root->outputs->items[i];
			arrange_layers(output);
		}
		return;
	}
	if (seat->focused_layer) {
		if (wl_resource_get_client(seat->focused_layer->resource) != client) {
			seat_set_focus_layer(seat, NULL);
		}
	}
	if (seat->has_focus) {
		struct sway_node *focus = seat_get_focus(seat);
		if (node_is_view(focus) && wl_resource_get_client(
					focus->sway_container->view->surface->resource) != client) {
			seat_set_focus(seat, NULL);
		}
	}
	if (seat->wlr_seat->pointer_state.focused_client) {
		if (seat->wlr_seat->pointer_state.focused_client->client != client) {
			wlr_seat_pointer_clear_focus(seat->wlr_seat);
		}
	}
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	struct wlr_touch_point *point;
	wl_list_for_each(point, &seat->wlr_seat->touch_state.touch_points, link) {
		if (point->client->client != client) {
			wlr_seat_touch_point_clear_focus(seat->wlr_seat,
					now.tv_nsec / 1000, point->touch_id);
		}
	}
	seat->exclusive_client = client;
}

struct sway_node *seat_get_focus_inactive(struct sway_seat *seat,
		struct sway_node *node) {
	if (node_is_view(node)) {
		return node;
	}
	struct sway_seat_node *current;
	wl_list_for_each(current, &seat->focus_stack, link) {
		if (node_has_ancestor(current->node, node)) {
			return current->node;
		}
	}
	if (node->type == N_WORKSPACE) {
		return node;
	}
	return NULL;
}

struct sway_container *seat_get_focus_inactive_tiling(struct sway_seat *seat,
		struct sway_workspace *workspace) {
	if (!workspace->tiling->length) {
		return NULL;
	}
	struct sway_seat_node *current;
	wl_list_for_each(current, &seat->focus_stack, link) {
		struct sway_node *node = current->node;
		if (node->type == N_CONTAINER &&
				!container_is_floating_or_child(node->sway_container) &&
				node->sway_container->workspace == workspace) {
			return node->sway_container;
		}
	}
	return NULL;
}

struct sway_container *seat_get_focus_inactive_floating(struct sway_seat *seat,
		struct sway_workspace *workspace) {
	if (!workspace->floating->length) {
		return NULL;
	}
	struct sway_seat_node *current;
	wl_list_for_each(current, &seat->focus_stack, link) {
		struct sway_node *node = current->node;
		if (node->type == N_CONTAINER &&
				container_is_floating_or_child(node->sway_container) &&
				node->sway_container->workspace == workspace) {
			return node->sway_container;
		}
	}
	return NULL;
}

struct sway_node *seat_get_active_child(struct sway_seat *seat,
		struct sway_node *parent) {
	if (node_is_view(parent)) {
		return parent;
	}
	struct sway_seat_node *current;
	wl_list_for_each(current, &seat->focus_stack, link) {
		struct sway_node *node = current->node;
		if (node_get_parent(node) == parent) {
			return node;
		}
	}
	return NULL;
}

struct sway_node *seat_get_focus(struct sway_seat *seat) {
	if (!seat->has_focus) {
		return NULL;
	}
	struct sway_seat_node *current =
		wl_container_of(seat->focus_stack.next, current, link);
	return current->node;
}

struct sway_workspace *seat_get_focused_workspace(struct sway_seat *seat) {
	struct sway_node *focus = seat_get_focus(seat);
	if (!focus) {
		return NULL;
	}
	if (focus->type == N_CONTAINER) {
		return focus->sway_container->workspace;
	}
	if (focus->type == N_WORKSPACE) {
		return focus->sway_workspace;
	}
	return NULL; // unreachable
}

struct sway_container *seat_get_focused_container(struct sway_seat *seat) {
	struct sway_node *focus = seat_get_focus(seat);
	if (focus && focus->type == N_CONTAINER) {
		return focus->sway_container;
	}
	return NULL;
}

void seat_apply_config(struct sway_seat *seat,
		struct seat_config *seat_config) {
	struct sway_seat_device *seat_device = NULL;

	if (!seat_config) {
		return;
	}

	wl_list_for_each(seat_device, &seat->devices, link) {
		seat_configure_device(seat, seat_device->input_device);
	}
}

struct seat_config *seat_get_config(struct sway_seat *seat) {
	struct seat_config *seat_config = NULL;
	for (int i = 0; i < config->seat_configs->length; ++i ) {
		seat_config = config->seat_configs->items[i];
		if (strcmp(seat->wlr_seat->name, seat_config->name) == 0) {
			return seat_config;
		}
	}

	return NULL;
}

void seat_begin_down(struct sway_seat *seat, struct sway_container *con,
		uint32_t button, double sx, double sy) {
	seat->operation = OP_DOWN;
	seat->op_container = con;
	seat->op_button = button;
	seat->op_ref_lx = seat->cursor->cursor->x;
	seat->op_ref_ly = seat->cursor->cursor->y;
	seat->op_ref_con_lx = sx;
	seat->op_ref_con_ly = sy;
	seat->op_moved = false;
}

void seat_begin_move_floating(struct sway_seat *seat,
		struct sway_container *con, uint32_t button) {
	if (!seat->cursor) {
		wlr_log(WLR_DEBUG, "Ignoring move request due to no cursor device");
		return;
	}
	seat->operation = OP_MOVE_FLOATING;
	seat->op_container = con;
	seat->op_button = button;
	cursor_set_image(seat->cursor, "grab", NULL);
}

void seat_begin_move_tiling(struct sway_seat *seat,
		struct sway_container *con, uint32_t button) {
	seat->operation = OP_MOVE_TILING;
	seat->op_container = con;
	seat->op_button = button;
	seat->op_target_node = NULL;
	seat->op_target_edge = 0;
	cursor_set_image(seat->cursor, "grab", NULL);
}

void seat_begin_resize_floating(struct sway_seat *seat,
		struct sway_container *con, uint32_t button, enum wlr_edges edge) {
	if (!seat->cursor) {
		wlr_log(WLR_DEBUG, "Ignoring resize request due to no cursor device");
		return;
	}
	struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat->wlr_seat);
	seat->operation = OP_RESIZE_FLOATING;
	seat->op_container = con;
	seat->op_resize_preserve_ratio = keyboard &&
		(wlr_keyboard_get_modifiers(keyboard) & WLR_MODIFIER_SHIFT);
	seat->op_resize_edge = edge == WLR_EDGE_NONE ?
		WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT : edge;
	seat->op_button = button;
	seat->op_ref_lx = seat->cursor->cursor->x;
	seat->op_ref_ly = seat->cursor->cursor->y;
	seat->op_ref_con_lx = con->x;
	seat->op_ref_con_ly = con->y;
	seat->op_ref_width = con->width;
	seat->op_ref_height = con->height;

	const char *image = edge == WLR_EDGE_NONE ?
		"se-resize" : wlr_xcursor_get_resize_name(edge);
	cursor_set_image(seat->cursor, image, NULL);
}

void seat_begin_resize_tiling(struct sway_seat *seat,
		struct sway_container *con, uint32_t button, enum wlr_edges edge) {
	seat->operation = OP_RESIZE_TILING;
	seat->op_container = con;
	seat->op_resize_edge = edge;
	seat->op_button = button;
	seat->op_ref_lx = seat->cursor->cursor->x;
	seat->op_ref_ly = seat->cursor->cursor->y;
	seat->op_ref_con_lx = con->x;
	seat->op_ref_con_ly = con->y;
	seat->op_ref_width = con->width;
	seat->op_ref_height = con->height;
}

static bool is_parallel(enum sway_container_layout layout,
		enum wlr_edges edge) {
	bool layout_is_horiz = layout == L_HORIZ || layout == L_TABBED;
	bool edge_is_horiz = edge == WLR_EDGE_LEFT || edge == WLR_EDGE_RIGHT;
	return layout_is_horiz == edge_is_horiz;
}

static void seat_end_move_tiling(struct sway_seat *seat) {
	struct sway_container *con = seat->op_container;
	struct sway_container *old_parent = con->parent;
	struct sway_workspace *old_ws = con->workspace;
	struct sway_node *target_node = seat->op_target_node;
	struct sway_workspace *new_ws = target_node->type == N_WORKSPACE ?
		target_node->sway_workspace : target_node->sway_container->workspace;
	enum wlr_edges edge = seat->op_target_edge;
	int after = edge != WLR_EDGE_TOP && edge != WLR_EDGE_LEFT;

	container_detach(con);
	if (old_parent) {
		container_reap_empty(old_parent);
	}

	// Moving container into empty workspace
	if (target_node->type == N_WORKSPACE && edge == WLR_EDGE_NONE) {
		workspace_add_tiling(new_ws, con);
	} else if (target_node->type == N_CONTAINER) {
		// Moving container before/after another
		struct sway_container *target = target_node->sway_container;
		enum sway_container_layout layout = container_parent_layout(target);
		if (edge && !is_parallel(layout, edge)) {
			enum sway_container_layout new_layout = edge == WLR_EDGE_TOP ||
				edge == WLR_EDGE_BOTTOM ? L_VERT : L_HORIZ;
			container_split(target, new_layout);
		}
		container_add_sibling(target, con, after);
	} else {
		// Target is a workspace which requires splitting
		enum sway_container_layout new_layout = edge == WLR_EDGE_TOP ||
			edge == WLR_EDGE_BOTTOM ? L_VERT : L_HORIZ;
		workspace_split(new_ws, new_layout);
		workspace_insert_tiling(new_ws, con, after);
	}

	// This is a bit dirty, but we'll set the dimensions to that of a sibling.
	// I don't think there's any other way to make it consistent without
	// changing how we auto-size containers.
	list_t *siblings = container_get_siblings(con);
	if (siblings->length > 1) {
		int index = list_find(siblings, con);
		struct sway_container *sibling = index == 0 ?
			siblings->items[1] : siblings->items[index - 1];
		con->width = sibling->width;
		con->height = sibling->height;
	}

	arrange_workspace(old_ws);
	if (new_ws != old_ws) {
		arrange_workspace(new_ws);
	}
}

void seat_end_mouse_operation(struct sway_seat *seat) {
	enum sway_seat_operation operation = seat->operation;
	if (seat->operation == OP_MOVE_FLOATING) {
		// We "move" the container to its own location so it discovers its
		// output again.
		struct sway_container *con = seat->op_container;
		container_floating_move_to(con, con->x, con->y);
	} else if (seat->operation == OP_MOVE_TILING && seat->op_target_node) {
		seat_end_move_tiling(seat);
	}
	seat->operation = OP_NONE;
	seat->op_container = NULL;
	if (operation == OP_DOWN) {
		// Set the cursor's previous coords to the x/y at the start of the
		// operation, so the container change will be detected if using
		// focus_follows_mouse and the cursor moved off the original container
		// during the operation.
		seat->cursor->previous.x = seat->op_ref_lx;
		seat->cursor->previous.y = seat->op_ref_ly;
		if (seat->op_moved) {
			cursor_send_pointer_motion(seat->cursor, 0, true);
		}
	} else {
		cursor_set_image(seat->cursor, "left_ptr", NULL);
	}
}

void seat_pointer_notify_button(struct sway_seat *seat, uint32_t time_msec,
		uint32_t button, enum wlr_button_state state) {
	seat->last_button = button;
	seat->last_button_serial = wlr_seat_pointer_notify_button(seat->wlr_seat,
			time_msec, button, state);
}
