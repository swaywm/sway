#define _XOPEN_SOURCE 700
#include <wlr/types/wlr_cursor.h>
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

void sway_seat_destroy(struct sway_seat *seat) {
	struct sway_seat_device *seat_device, *next;
	wl_list_for_each_safe(seat_device, next, &seat->devices, link) {
		seat_device_destroy(seat_device);
	}
	sway_cursor_destroy(seat->cursor);
	wl_list_remove(&seat->link);
	wlr_seat_destroy(seat->wlr_seat);
}

static void handle_seat_container_destroy(struct wl_listener *listener,
		void *data) {
	struct sway_seat_container *seat_con =
		wl_container_of(listener, seat_con, destroy);
	struct sway_seat *seat = seat_con->seat;
	struct sway_container *con = seat_con->container;

	bool is_focus = (sway_seat_get_focus(seat) == con);

	wl_list_remove(&seat_con->link);

	if  (is_focus) {
		// pick next focus
		sway_seat_set_focus(seat, NULL);
		struct sway_container *next = sway_seat_get_focus_inactive(seat, con->parent);
		if (next == NULL) {
			next = con->parent;
		}
		sway_seat_set_focus(seat, next);
	}

	wl_list_remove(&seat_con->destroy.link);

	free(seat_con);
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

struct sway_seat *sway_seat_create(struct sway_input_manager *input,
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

	container_for_each_descendant_dfs(&root_container, collect_focus_iter, seat);

	wl_signal_add(&root_container.sway_root->events.new_container,
		&seat->new_container);
	seat->new_container.notify = handle_new_container;

	seat->input = input;
	wl_list_init(&seat->devices);

	wlr_seat_set_capabilities(seat->wlr_seat,
		WL_SEAT_CAPABILITY_KEYBOARD |
		WL_SEAT_CAPABILITY_POINTER |
		WL_SEAT_CAPABILITY_TOUCH);

	sway_seat_configure_xcursor(seat);

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
	struct wlr_keyboard *wlr_keyboard = seat_device->input_device->wlr_device->keyboard;
	sway_keyboard_configure(seat_device->keyboard);
	wlr_seat_set_keyboard(seat->wlr_seat,
			seat_device->input_device->wlr_device);
	struct sway_container *focus = sway_seat_get_focus(seat);
	if (focus && focus->type == C_VIEW) {
		// force notify reenter to pick up the new configuration
		wlr_seat_keyboard_clear_focus(seat->wlr_seat);
		wlr_seat_keyboard_notify_enter(seat->wlr_seat,
				focus->sway_view->surface, wlr_keyboard->keycodes,
				wlr_keyboard->num_keycodes, &wlr_keyboard->modifiers);
	}
}

static struct sway_seat_device *sway_seat_get_device(struct sway_seat *seat,
		struct sway_input_device *input_device) {
	struct sway_seat_device *seat_device = NULL;
	wl_list_for_each(seat_device, &seat->devices, link) {
		if (seat_device->input_device == input_device) {
			return seat_device;
		}
	}

	return NULL;
}

void sway_seat_configure_device(struct sway_seat *seat,
		struct sway_input_device *input_device) {
	struct sway_seat_device *seat_device =
		sway_seat_get_device(seat, input_device);
	if (!seat_device) {
		return;
	}

	if (seat->config) {
		seat_device->attachment_config =
			seat_config_get_attachment(seat->config, input_device->identifier);
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

void sway_seat_add_device(struct sway_seat *seat,
		struct sway_input_device *input_device) {
	if (sway_seat_get_device(seat, input_device)) {
		sway_seat_configure_device(seat, input_device);
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

	sway_seat_configure_device(seat, input_device);
}

void sway_seat_remove_device(struct sway_seat *seat,
		struct sway_input_device *input_device) {
	struct sway_seat_device *seat_device =
		sway_seat_get_device(seat, input_device);

	if (!seat_device) {
		return;
	}

	wlr_log(L_DEBUG, "removing device %s from seat %s",
		input_device->identifier, seat->wlr_seat->name);

	seat_device_destroy(seat_device);
}

void sway_seat_configure_xcursor(struct sway_seat *seat) {
	// TODO configure theme and size
	const char *cursor_theme = "default";

	if (!seat->cursor->xcursor_manager) {
		seat->cursor->xcursor_manager =
			wlr_xcursor_manager_create("default", 24);
		if (sway_assert(seat->cursor->xcursor_manager,
					"Cannot create XCursor manager for theme %s",
					cursor_theme)) {
			return;
		}
	}

	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *output_container = root_container.children->items[i];
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

void sway_seat_set_focus(struct sway_seat *seat, struct sway_container *container) {
	struct sway_container *last_focus = sway_seat_get_focus(seat);

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
			struct sway_view *view = container->sway_view;
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
	}

	if (last_focus) {
		struct sway_container *last_ws = last_focus;
		if (last_ws && last_ws->type != C_WORKSPACE) {
			last_ws = container_parent(last_focus, C_WORKSPACE);
		}
		if (last_ws) {
			wlr_log(L_DEBUG, "sending workspace event");
			ipc_event_workspace(last_ws, container, "focus");
			if (last_ws->children->length == 0) {
				container_workspace_destroy(last_ws);
			}
		}
	}

	if (last_focus && last_focus->type == C_VIEW &&
			!sway_input_manager_has_focus(seat->input, last_focus)) {
		struct sway_view *view = last_focus->sway_view;
		view_set_activated(view, false);
	}

	seat->has_focus = (container != NULL);
}

struct sway_container *sway_seat_get_focus_inactive(struct sway_seat *seat, struct sway_container *container) {
	struct sway_seat_container *current = NULL;
	struct sway_container *parent = NULL;
	wl_list_for_each(current, &seat->focus_stack, link) {
		parent = current->container->parent;

		if (current->container == container) {
			return current->container;
		}

		while (parent) {
			if (parent == container) {
				return current->container;
			}
			parent = parent->parent;
		}
	}

	return NULL;
}

struct sway_container *sway_seat_get_focus(struct sway_seat *seat) {
	if (!seat->has_focus) {
		return NULL;
	}
	return sway_seat_get_focus_inactive(seat, &root_container);
}

struct sway_container *sway_seat_get_focus_by_type(struct sway_seat *seat,
		enum sway_container_type type) {
	struct sway_container *focus = sway_seat_get_focus_inactive(seat, &root_container);
	if (focus->type == type) {
		return focus;
	}

	return container_parent(focus, type);
}

void sway_seat_set_config(struct sway_seat *seat,
		struct seat_config *seat_config) {
	// clear configs
	free_seat_config(seat->config);
	seat->config = NULL;

	struct sway_seat_device *seat_device = NULL;
	wl_list_for_each(seat_device, &seat->devices, link) {
		seat_device->attachment_config = NULL;
	}

	if (!seat_config) {
		return;
	}

	// add configs
	seat->config = copy_seat_config(seat_config);

	wl_list_for_each(seat_device, &seat->devices, link) {
		sway_seat_configure_device(seat, seat_device->input_device);
	}
}
