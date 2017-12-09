#define _XOPEN_SOURCE 700
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include "sway/input/seat.h"
#include "sway/input/cursor.h"
#include "sway/input/input-manager.h"
#include "sway/output.h"
#include "log.h"

struct sway_seat *sway_seat_create(struct wl_display *display,
		const char *seat_name) {
	struct sway_seat *seat = calloc(1, sizeof(struct sway_seat));
	if (!seat) {
		return NULL;
	}

	seat->seat = wlr_seat_create(display, seat_name);
	if (!sway_assert(seat->seat, "could not allocate seat")) {
		return NULL;
	}

	seat->cursor = sway_cursor_create(seat);
	if (!seat->cursor) {
		wlr_seat_destroy(seat->seat);
		free(seat);
		return NULL;
	}

	wlr_seat_set_capabilities(seat->seat,
		WL_SEAT_CAPABILITY_KEYBOARD |
		WL_SEAT_CAPABILITY_POINTER |
		WL_SEAT_CAPABILITY_TOUCH);

	sway_seat_configure_xcursor(seat);

	return seat;
}

static void seat_add_pointer(struct sway_seat *seat,
		struct wlr_input_device *device) {
	// TODO pointer configuration
	wlr_cursor_attach_input_device(seat->cursor->cursor, device);
}

void sway_seat_add_device(struct sway_seat *seat,
		struct wlr_input_device *device) {
	sway_log(L_DEBUG, "input add: %s", device->name);
	switch (device->type) {
		case WLR_INPUT_DEVICE_POINTER:
			seat_add_pointer(seat, device);
			break;
		case WLR_INPUT_DEVICE_KEYBOARD:
		case WLR_INPUT_DEVICE_TOUCH:
		case WLR_INPUT_DEVICE_TABLET_PAD:
		case WLR_INPUT_DEVICE_TABLET_TOOL:
			sway_log(L_DEBUG, "TODO: add other devices");
			break;
	}
}

static void seat_remove_pointer(struct sway_seat *seat,
		struct wlr_input_device *device) {
	// TODO
}

void sway_seat_remove_device(struct sway_seat *seat,
		struct wlr_input_device *device) {
	sway_log(L_DEBUG, "input remove: %s", device->name);
	switch (device->type) {
		case WLR_INPUT_DEVICE_POINTER:
			seat_remove_pointer(seat, device);
			break;
		case WLR_INPUT_DEVICE_KEYBOARD:
		case WLR_INPUT_DEVICE_TOUCH:
		case WLR_INPUT_DEVICE_TABLET_PAD:
		case WLR_INPUT_DEVICE_TABLET_TOOL:
			sway_log(L_DEBUG, "TODO: remove other devices");
			break;
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
