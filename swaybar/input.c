#include <assert.h>
#ifdef __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#else
#include <linux/input-event-codes.h>
#endif
#include <stdlib.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wlr/util/log.h>
#include "list.h"
#include "log.h"
#include "swaybar/bar.h"
#include "swaybar/config.h"
#include "swaybar/input.h"
#include "swaybar/ipc.h"

void free_hotspots(struct wl_list *list) {
	struct swaybar_hotspot *hotspot, *tmp;
	wl_list_for_each_safe(hotspot, tmp, list, link) {
		wl_list_remove(&hotspot->link);
		if (hotspot->destroy) {
			hotspot->destroy(hotspot->data);
		}
		free(hotspot);
	}
}

static enum x11_button wl_button_to_x11_button(uint32_t button) {
	switch (button) {
	case BTN_LEFT:
		return LEFT;
	case BTN_MIDDLE:
		return MIDDLE;
	case BTN_RIGHT:
		return RIGHT;
	case BTN_SIDE:
		return BACK;
	case BTN_EXTRA:
		return FORWARD;
	default:
		return NONE;
	}
}

static enum x11_button wl_axis_to_x11_button(uint32_t axis, wl_fixed_t value) {
	switch (axis) {
	case WL_POINTER_AXIS_VERTICAL_SCROLL:
		return wl_fixed_to_double(value) < 0 ? SCROLL_UP : SCROLL_DOWN;
	case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
		return wl_fixed_to_double(value) < 0 ? SCROLL_LEFT : SCROLL_RIGHT;
	default:
		wlr_log(WLR_DEBUG, "Unexpected axis value on mouse scroll");
		return NONE;
	}
}

static void wl_pointer_enter(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface,
		wl_fixed_t surface_x, wl_fixed_t surface_y) {
	struct swaybar *bar = data;
	struct swaybar_pointer *pointer = &bar->pointer;
	struct swaybar_output *output;
	wl_list_for_each(output, &bar->outputs, link) {
		if (output->surface == surface) {
			pointer->current = output;
			break;
		}
	}
	int max_scale = 1;
	struct swaybar_output *_output;
	wl_list_for_each(_output, &bar->outputs, link) {
		if (_output->scale > max_scale) {
			max_scale = _output->scale;
		}
	}
	wl_surface_set_buffer_scale(pointer->cursor_surface, max_scale);
	wl_surface_attach(pointer->cursor_surface,
			wl_cursor_image_get_buffer(pointer->cursor_image), 0, 0);
	wl_pointer_set_cursor(wl_pointer, serial, pointer->cursor_surface,
			pointer->cursor_image->hotspot_x / max_scale,
			pointer->cursor_image->hotspot_y / max_scale);
	wl_surface_commit(pointer->cursor_surface);
}

static void wl_pointer_leave(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface) {
	struct swaybar *bar = data;
	bar->pointer.current = NULL;
}

static void wl_pointer_motion(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	struct swaybar *bar = data;
	bar->pointer.x = wl_fixed_to_int(surface_x);
	bar->pointer.y = wl_fixed_to_int(surface_y);
}

static bool check_bindings(struct swaybar *bar, uint32_t x11_button,
		uint32_t state) {
	bool released = state == WL_POINTER_BUTTON_STATE_RELEASED;
	for (int i = 0; i < bar->config->bindings->length; i++) {
		struct swaybar_binding *binding = bar->config->bindings->items[i];
		if (binding->button == x11_button && binding->release == released) {
			ipc_execute_binding(bar, binding);
			return true;
		}
	}
	return false;
}

static void wl_pointer_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
	struct swaybar *bar = data;
	struct swaybar_pointer *pointer = &bar->pointer;
	struct swaybar_output *output = pointer->current;
	if (!sway_assert(output, "button with no active output")) {
		return;
	}

	if (check_bindings(bar, wl_button_to_x11_button(button), state)) {
		return;
	}

	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		return;
	}
	struct swaybar_hotspot *hotspot;
	wl_list_for_each(hotspot, &output->hotspots, link) {
		double x = pointer->x * output->scale;
		double y = pointer->y * output->scale;
		if (x >= hotspot->x
				&& y >= hotspot->y
				&& x < hotspot->x + hotspot->width
				&& y < hotspot->y + hotspot->height) {
			if (HOTSPOT_IGNORE == hotspot->callback(output, pointer->x, pointer->y,
					wl_button_to_x11_button(button), hotspot->data)) {
				return;
			}
		}
	}
}

static void wl_pointer_axis(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis, wl_fixed_t value) {
	struct swaybar *bar = data;
	struct swaybar_pointer *pointer = &bar->pointer;
	struct swaybar_output *output = pointer->current;
	if (!sway_assert(output, "axis with no active output")) {
		return;
	}

	// If there is a button press binding, execute it, skip default behavior,
	// and check button release bindings
	enum x11_button button = wl_axis_to_x11_button(axis, value);
	if (check_bindings(bar, button, WL_POINTER_BUTTON_STATE_PRESSED)) {
		check_bindings(bar, button, WL_POINTER_BUTTON_STATE_RELEASED);
		return;
	}

	struct swaybar_hotspot *hotspot;
	wl_list_for_each(hotspot, &output->hotspots, link) {
		double x = pointer->x * output->scale;
		double y = pointer->y * output->scale;
		if (x >= hotspot->x
				&& y >= hotspot->y
				&& x < hotspot->x + hotspot->width
				&& y < hotspot->y + hotspot->height) {
			if (HOTSPOT_IGNORE == hotspot->callback(
					output, pointer->x, pointer->y, button, hotspot->data)) {
				return;
			}
		}
	}

	struct swaybar_config *config = bar->config;
	double amt = wl_fixed_to_double(value);
	if (amt == 0.0 || !config->workspace_buttons) {
		check_bindings(bar, button, WL_POINTER_BUTTON_STATE_RELEASED);
		return;
	}

	if (!sway_assert(!wl_list_empty(&output->workspaces), "axis with no workspaces")) {
		return;
	}

	struct swaybar_workspace *first =
		wl_container_of(output->workspaces.next, first, link);
	struct swaybar_workspace *last =
		wl_container_of(output->workspaces.prev, last, link);

	struct swaybar_workspace *active;
	wl_list_for_each(active, &output->workspaces, link) {
		if (active->visible) {
			break;
		}
	}
	if (!sway_assert(active->visible, "axis with null workspace")) {
		return;
	}

	struct swaybar_workspace *new;
	if (amt < 0.0) {
		if (active == first) {
			new = config->wrap_scroll ? last : NULL;
		} else {
			new = wl_container_of(active->link.prev, new, link);
		}
	} else {
		if (active == last) {
			new = config->wrap_scroll ? first : NULL;
		} else {
			new = wl_container_of(active->link.next, new, link);
		}
	}

	if (new) {
		ipc_send_workspace_command(bar, new->name);
	}

	// Check button release bindings
	check_bindings(bar, button, WL_POINTER_BUTTON_STATE_RELEASED);
}

static void wl_pointer_frame(void *data, struct wl_pointer *wl_pointer) {
	// Who cares
}

static void wl_pointer_axis_source(void *data, struct wl_pointer *wl_pointer,
		uint32_t axis_source) {
	// Who cares
}

static void wl_pointer_axis_stop(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis) {
	// Who cares
}

static void wl_pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer,
		uint32_t axis, int32_t discrete) {
	// Who cares
}

struct wl_pointer_listener pointer_listener = {
	.enter = wl_pointer_enter,
	.leave = wl_pointer_leave,
	.motion = wl_pointer_motion,
	.button = wl_pointer_button,
	.axis = wl_pointer_axis,
	.frame = wl_pointer_frame,
	.axis_source = wl_pointer_axis_source,
	.axis_stop = wl_pointer_axis_stop,
	.axis_discrete = wl_pointer_axis_discrete,
};

static void seat_handle_capabilities(void *data, struct wl_seat *wl_seat,
		enum wl_seat_capability caps) {
	struct swaybar *bar = data;
	if (bar->pointer.pointer != NULL) {
		wl_pointer_release(bar->pointer.pointer);
		bar->pointer.pointer = NULL;
	}
	if ((caps & WL_SEAT_CAPABILITY_POINTER)) {
		bar->pointer.pointer = wl_seat_get_pointer(wl_seat);
		wl_pointer_add_listener(bar->pointer.pointer, &pointer_listener, bar);
	}
}

static void seat_handle_name(void *data, struct wl_seat *wl_seat,
		const char *name) {
	// Who cares
}

const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
	.name = seat_handle_name,
};
