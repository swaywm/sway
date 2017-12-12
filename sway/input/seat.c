#define _XOPEN_SOURCE 700
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include "sway/input/seat.h"
#include "sway/input/cursor.h"
#include "sway/input/input-manager.h"
#include "sway/input/keyboard.h"
#include "sway/output.h"
#include "sway/view.h"
#include "log.h"

struct sway_seat *sway_seat_create(struct sway_input_manager *input,
		const char *seat_name) {
	struct sway_seat *seat = calloc(1, sizeof(struct sway_seat));
	if (!seat) {
		return NULL;
	}

	seat->seat = wlr_seat_create(input->server->wl_display, seat_name);
	if (!sway_assert(seat->seat, "could not allocate seat")) {
		return NULL;
	}

	seat->cursor = sway_cursor_create(seat);
	if (!seat->cursor) {
		wlr_seat_destroy(seat->seat);
		free(seat);
		return NULL;
	}

	seat->input = input;
	seat->devices = create_list();

	wlr_seat_set_capabilities(seat->seat,
		WL_SEAT_CAPABILITY_KEYBOARD |
		WL_SEAT_CAPABILITY_POINTER |
		WL_SEAT_CAPABILITY_TOUCH);

	sway_seat_configure_xcursor(seat);

	wl_list_insert(&input->seats, &seat->link);

	return seat;
}

static void seat_add_pointer(struct sway_seat *seat,
		struct sway_input_device *sway_device) {
	// TODO pointer configuration
	wlr_cursor_attach_input_device(seat->cursor->cursor,
		sway_device->wlr_device);
}

static void seat_add_keyboard(struct sway_seat *seat,
		struct sway_input_device *device) {
	// TODO keyboard configuration
	sway_keyboard_create(seat, device);
	wlr_seat_set_keyboard(seat->seat, device->wlr_device);
}

bool sway_seat_has_device(struct sway_seat *seat,
		struct sway_input_device *device) {
	return false;
}

void sway_seat_add_device(struct sway_seat *seat,
		struct sway_input_device *device) {
	if (sway_seat_has_device(seat, device)) {
		return;
	}

	sway_log(L_DEBUG, "input add: %s", device->identifier);
	switch (device->wlr_device->type) {
		case WLR_INPUT_DEVICE_POINTER:
			seat_add_pointer(seat, device);
			break;
		case WLR_INPUT_DEVICE_KEYBOARD:
			seat_add_keyboard(seat, device);
			break;
		case WLR_INPUT_DEVICE_TOUCH:
		case WLR_INPUT_DEVICE_TABLET_PAD:
		case WLR_INPUT_DEVICE_TABLET_TOOL:
			sway_log(L_DEBUG, "TODO: add other devices");
			break;
	}

	list_add(seat->devices, device);
}

static void seat_remove_keyboard(struct sway_seat *seat,
		struct sway_input_device *device) {
	if (device && device->keyboard) {
		sway_keyboard_destroy(device->keyboard);
	}
}

static void seat_remove_pointer(struct sway_seat *seat,
		struct sway_input_device *device) {
	wlr_cursor_detach_input_device(seat->cursor->cursor, device->wlr_device);
}

void sway_seat_remove_device(struct sway_seat *seat,
		struct sway_input_device *device) {
	sway_log(L_DEBUG, "input remove: %s", device->identifier);
	if (!sway_seat_has_device(seat, device)) {
		return;
	}

	switch (device->wlr_device->type) {
		case WLR_INPUT_DEVICE_POINTER:
			seat_remove_pointer(seat, device);
			break;
		case WLR_INPUT_DEVICE_KEYBOARD:
			seat_remove_keyboard(seat, device);
			break;
		case WLR_INPUT_DEVICE_TOUCH:
		case WLR_INPUT_DEVICE_TABLET_PAD:
		case WLR_INPUT_DEVICE_TABLET_TOOL:
			sway_log(L_DEBUG, "TODO: remove other devices");
			break;
	}

	for (int i = 0; i < seat->devices->length; ++i) {
		if (seat->devices->items[i] == device) {
			list_del(seat->devices, i);
			break;
		}
	}
}

void sway_seat_configure_xcursor(struct sway_seat *seat) {
	// TODO configure theme and size
	const char *cursor_theme = "default";

	if (!seat->cursor->xcursor_manager) {
		seat->cursor->xcursor_manager =
			wlr_xcursor_manager_create("default", 24);
		if (sway_assert(seat->cursor->xcursor_manager,
					"Cannot create XCursor manager for theme %s", cursor_theme)) {
			return;
		}
	}

	for (int i = 0; i < root_container.children->length; ++i) {
		swayc_t *output_container = root_container.children->items[i];
		struct wlr_output *output =
			output_container->sway_output->wlr_output;
		bool result =
			wlr_xcursor_manager_load(seat->cursor->xcursor_manager,
				output->scale);

		sway_assert(!result,
			"Cannot load xcursor theme for output '%s' with scale %d",
			output->name, output->scale);
	}

	wlr_xcursor_manager_set_cursor_image(seat->cursor->xcursor_manager,
		"left_ptr", seat->cursor->cursor);
	wlr_cursor_warp(seat->cursor->cursor, NULL, seat->cursor->cursor->x,
		seat->cursor->cursor->y);
}

static void handle_focus_destroy(struct wl_listener *listener, void *data) {
	struct sway_seat *seat = wl_container_of(listener, seat, focus_destroy);
	//swayc_t *container = data;

	// TODO set new focus based on the state of the tree
	sway_seat_set_focus(seat, NULL);
}

void sway_seat_set_focus(struct sway_seat *seat, swayc_t *container) {
	swayc_t *last_focus = seat->focus;

	if (last_focus == container) {
		return;
	}

	if (last_focus) {
		wl_list_remove(&seat->focus_destroy.link);
	}

	if (container) {
		struct sway_view *view = container->sway_view;
		view->iface.set_activated(view, true);
		wl_signal_add(&container->events.destroy, &seat->focus_destroy);
		seat->focus_destroy.notify = handle_focus_destroy;
		wlr_seat_keyboard_notify_enter(seat->seat, view->surface);
	}

	seat->focus = container;

	if (last_focus &&
			!sway_input_manager_has_focus(seat->input, last_focus)) {
		struct sway_view *view = last_focus->sway_view;
		view->iface.set_activated(view, false);

	}
}
