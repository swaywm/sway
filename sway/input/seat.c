#define _XOPEN_SOURCE 700
#include <assert.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include "sway/tree/container.h"
#include "sway/input/seat.h"
#include "sway/input/cursor.h"
#include "sway/input/input-manager.h"
#include "sway/input/keyboard.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "log.h"

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
	wl_list_remove(&seat->link);
	wlr_seat_destroy(seat->wlr_seat);
}

static struct sway_seat_container *seat_container_from_container(
		struct sway_seat *seat, struct sway_container *con);

static void seat_container_destroy(struct sway_seat_container *seat_con) {
	struct sway_container *con = seat_con->container;
	struct sway_container *child = NULL;

	if (con->children != NULL) {
		for (int i = 0; i < con->children->length; ++i) {
			child = con->children->items[i];
			struct sway_seat_container *seat_child =
				seat_container_from_container(seat_con->seat, child);
			seat_container_destroy(seat_child);
		}
	}

	wl_list_remove(&seat_con->destroy.link);
	wl_list_remove(&seat_con->link);
	free(seat_con);
}

static void seat_send_focus(struct sway_seat *seat,
		struct sway_container *con) {
	if (con->type != C_VIEW) {
		return;
	}
	struct sway_view *view = con->sway_view;
	if (view->type == SWAY_XWAYLAND_VIEW) {
		struct wlr_xwayland *xwayland =
			seat->input->server->xwayland;
		wlr_xwayland_set_seat(xwayland, seat->wlr_seat);
	}
	view_set_activated(view, true);
	struct wlr_keyboard *keyboard =
		wlr_seat_get_keyboard(seat->wlr_seat);
	if (keyboard) {
		wlr_seat_keyboard_notify_enter(seat->wlr_seat,
				view->surface, keyboard->keycodes,
				keyboard->num_keycodes, &keyboard->modifiers);
	} else {
		wlr_seat_keyboard_notify_enter(
				seat->wlr_seat, view->surface, NULL, 0, NULL);
	}

}

static void handle_seat_container_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_seat_container *seat_con =
		wl_container_of(listener, seat_con, destroy);
	struct sway_seat *seat = seat_con->seat;
	struct sway_container *con = seat_con->container;
	struct sway_container *parent = con->parent;
	struct sway_container *focus = seat_get_focus(seat);

	bool set_focus =
		focus != NULL &&
		(focus == con || container_has_child(con, focus)) &&
		con->type != C_WORKSPACE;

	seat_container_destroy(seat_con);

	if (set_focus) {
		struct sway_container *next_focus = NULL;
		while (next_focus == NULL) {
			next_focus = seat_get_focus_by_type(seat, parent, C_VIEW);

			if (next_focus == NULL && parent->type == C_WORKSPACE) {
				next_focus = parent;
				break;
			}

			parent = parent->parent;
		}

		// the structure change might have caused it to move up to the top of
		// the focus stack without sending focus notifications to the view
		if (seat_get_focus(seat) == next_focus) {
			seat_send_focus(seat, next_focus);
		} else {
			seat_set_focus(seat, next_focus);
		}
	}
}

static struct sway_seat_container *seat_container_from_container(
		struct sway_seat *seat, struct sway_container *con) {
	if (con->type < C_WORKSPACE) {
		// these don't get seat containers ever
		return NULL;
	}

	struct sway_seat_container *seat_con = NULL;
	wl_list_for_each(seat_con, &seat->focus_stack, link) {
		if (seat_con->container == con) {
			return seat_con;
		}
	}

	seat_con = calloc(1, sizeof(struct sway_seat_container));
	if (seat_con == NULL) {
		wlr_log(L_ERROR, "could not allocate seat container");
		return NULL;
	}

	seat_con->container = con;
	seat_con->seat = seat;
	wl_list_insert(seat->focus_stack.prev, &seat_con->link);
	wl_signal_add(&con->events.destroy, &seat_con->destroy);
	seat_con->destroy.notify = handle_seat_container_destroy;

	return seat_con;
}

static void handle_new_container(struct wl_listener *listener, void *data) {
	struct sway_seat *seat = wl_container_of(listener, seat, new_container);
	struct sway_container *con = data;
	seat_container_from_container(seat, con);
}

static void collect_focus_iter(struct sway_container *con, void *data) {
	struct sway_seat *seat = data;
	if (con->type > C_WORKSPACE) {
		return;
	}
	struct sway_seat_container *seat_con =
		seat_container_from_container(seat, con);
	if (!seat_con) {
		return;
	}
	wl_list_remove(&seat_con->link);
	wl_list_insert(&seat->focus_stack, &seat_con->link);
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

	seat->cursor = sway_cursor_create(seat);
	if (!seat->cursor) {
		wlr_seat_destroy(seat->wlr_seat);
		free(seat);
		return NULL;
	}

	// init the focus stack
	wl_list_init(&seat->focus_stack);

	container_for_each_descendant_dfs(&root_container,
		collect_focus_iter, seat);

	wl_signal_add(&root_container.sway_root->events.new_container,
		&seat->new_container);
	seat->new_container.notify = handle_new_container;

	seat->input = input;
	wl_list_init(&seat->devices);

	wlr_seat_set_capabilities(seat->wlr_seat,
		WL_SEAT_CAPABILITY_KEYBOARD |
		WL_SEAT_CAPABILITY_POINTER |
		WL_SEAT_CAPABILITY_TOUCH);

	seat_configure_xcursor(seat);

	wl_list_insert(&input->seats, &seat->link);

	return seat;
}

static void seat_configure_pointer(struct sway_seat *seat,
		struct sway_seat_device *sway_device) {
	wlr_cursor_attach_input_device(seat->cursor->cursor,
		sway_device->input_device->wlr_device);
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
	struct sway_container *focus = seat_get_focus(seat);
	if (focus && focus->type == C_VIEW) {
		// force notify reenter to pick up the new configuration
		wlr_seat_keyboard_clear_focus(seat->wlr_seat);
		wlr_seat_keyboard_notify_enter(seat->wlr_seat,
				focus->sway_view->surface, wlr_keyboard->keycodes,
				wlr_keyboard->num_keycodes, &wlr_keyboard->modifiers);
	}
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
	struct sway_seat_device *seat_device =
		seat_get_device(seat, input_device);
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
		case WLR_INPUT_DEVICE_TABLET_PAD:
		case WLR_INPUT_DEVICE_TABLET_TOOL:
			wlr_log(L_DEBUG, "TODO: configure other devices");
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
		wlr_log(L_DEBUG, "could not allocate seat device");
		return;
	}

	wlr_log(L_DEBUG, "adding device %s to seat %s",
		input_device->identifier, seat->wlr_seat->name);

	seat_device->sway_seat = seat;
	seat_device->input_device = input_device;
	wl_list_insert(&seat->devices, &seat_device->link);

	seat_configure_device(seat, input_device);
}

void seat_remove_device(struct sway_seat *seat,
		struct sway_input_device *input_device) {
	struct sway_seat_device *seat_device =
		seat_get_device(seat, input_device);

	if (!seat_device) {
		return;
	}

	wlr_log(L_DEBUG, "removing device %s from seat %s",
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

	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *output_container =
			root_container.children->items[i];
		struct wlr_output *output =
			output_container->sway_output->wlr_output;
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

void seat_set_focus_warp(struct sway_seat *seat,
		struct sway_container *container, bool warp) {
	struct sway_container *last_focus = seat_get_focus(seat);

	if (container && last_focus == container) {
		return;
	}

	if (container) {
		struct sway_seat_container *seat_con =
			seat_container_from_container(seat, container);
		if (!seat_con) {
			return;
		}

		wl_list_remove(&seat_con->link);
		wl_list_insert(&seat->focus_stack, &seat_con->link);

		if (container->type == C_VIEW) {
			seat_send_focus(seat, container);
		}
	}

	if (last_focus) {
		struct sway_container *last_ws = last_focus;
		if (last_ws && last_ws->type != C_WORKSPACE) {
			last_ws = container_parent(last_ws, C_WORKSPACE);
		}
		if (last_ws) {
			ipc_event_workspace(last_ws, container, "focus");
			if (last_ws->children->length == 0) {
				container_workspace_destroy(last_ws);
			}
		}
		struct sway_container *last_output = last_focus;
		if (last_output && last_output->type != C_OUTPUT) {
			last_output = container_parent(last_output, C_OUTPUT);
		}
		struct sway_container *new_output = container;
		if (new_output && new_output->type != C_OUTPUT) {
			new_output = container_parent(new_output, C_OUTPUT);
		}
		if (new_output && last_output && new_output != last_output
				&& config->mouse_warping && warp) {
			struct wlr_output *output = new_output->sway_output->wlr_output;
			double x = container->x + output->lx + container->width / 2.0;
			double y = container->y + output->ly + container->height / 2.0;
			if (!wlr_output_layout_contains_point(
					root_container.sway_root->output_layout,
					output, seat->cursor->cursor->x, seat->cursor->cursor->y)) {
				wlr_cursor_warp(seat->cursor->cursor, NULL, x, y);
			}
		}
	}

	if (last_focus && last_focus->type == C_VIEW &&
			!input_manager_has_focus(seat->input, last_focus)) {
		struct sway_view *view = last_focus->sway_view;
		view_set_activated(view, false);
	}

	seat->has_focus = (container != NULL);
}

void seat_set_focus(struct sway_seat *seat,
		struct sway_container *container) {
	seat_set_focus_warp(seat, container, true);
}

struct sway_container *seat_get_focus_inactive(struct sway_seat *seat,
		struct sway_container *container) {
	return seat_get_focus_by_type(seat, container, C_TYPES);
}

struct sway_container *sway_seat_get_focus(struct sway_seat *seat) {
	if (!seat->has_focus) {
		return NULL;
	}
	return seat_get_focus_inactive(seat, &root_container);
}

struct sway_container *seat_get_focus_by_type(struct sway_seat *seat,
		struct sway_container *container, enum sway_container_type type) {
	struct sway_seat_container *current = NULL;
	struct sway_container *parent = NULL;
	wl_list_for_each(current, &seat->focus_stack, link) {
		parent = current->container->parent;

		if (current->container == container &&
				(type == C_TYPES || container->type == type)) {
			return current->container;
		}

		while (parent) {
			if (parent == container && (type == C_TYPES ||
						current->container->type == type)) {
				return current->container;
			}
			parent = parent->parent;
		}
	}

	return NULL;
}

struct sway_container *seat_get_focus(struct sway_seat *seat) {
	if (!seat->has_focus) {
		return NULL;
	}
	return seat_get_focus_inactive(seat, &root_container);
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
