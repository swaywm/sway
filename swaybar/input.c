#include <assert.h>
#include <linux/input-event-codes.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include "list.h"
#include "log.h"
#include "swaybar/bar.h"
#include "swaybar/config.h"
#include "swaybar/input.h"
#include "swaybar/ipc.h"
#include "pointer-gestures-unstable-v1-client-protocol.h"

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

uint32_t event_to_x11_button(uint32_t event) {
	switch (event) {
	case BTN_LEFT:
		return 1;
	case BTN_MIDDLE:
		return 2;
	case BTN_RIGHT:
		return 3;
	case SWAY_SCROLL_UP:
		return 4;
	case SWAY_SCROLL_DOWN:
		return 5;
	case SWAY_SCROLL_LEFT:
		return 6;
	case SWAY_SCROLL_RIGHT:
		return 7;
	case BTN_SIDE:
		return 8;
	case BTN_EXTRA:
		return 9;
	default:
		return 0;
	}
}

static uint32_t wl_axis_to_button(uint32_t axis, wl_fixed_t value) {
	bool negative = wl_fixed_to_double(value) < 0;
	switch (axis) {
	case WL_POINTER_AXIS_VERTICAL_SCROLL:
		return negative ? SWAY_SCROLL_UP : SWAY_SCROLL_DOWN;
	case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
		return negative ? SWAY_SCROLL_LEFT : SWAY_SCROLL_RIGHT;
	default:
		sway_log(SWAY_DEBUG, "Unexpected axis value on mouse scroll");
		return 0;
	}
}

void update_cursor(struct swaybar_seat *seat) {
	struct swaybar_pointer *pointer = &seat->pointer;
	if (!pointer || !pointer->cursor_surface) {
		return;
	}
	if (pointer->cursor_theme) {
		wl_cursor_theme_destroy(pointer->cursor_theme);
	}
	const char *cursor_theme = getenv("XCURSOR_THEME");
	unsigned cursor_size = 24;
	const char *env_cursor_size = getenv("XCURSOR_SIZE");
	if (env_cursor_size && strlen(env_cursor_size) > 0) {
		errno = 0;
		char *end;
		unsigned size = strtoul(env_cursor_size, &end, 10);
		if (!*end && errno == 0) {
			cursor_size = size;
		}
	}
	int scale = pointer->current ? pointer->current->scale : 1;
	pointer->cursor_theme = wl_cursor_theme_load(
		cursor_theme, cursor_size * scale, seat->bar->shm);
	struct wl_cursor *cursor;
	cursor = wl_cursor_theme_get_cursor(pointer->cursor_theme, "left_ptr");
	pointer->cursor_image = cursor->images[0];
	wl_surface_set_buffer_scale(pointer->cursor_surface, scale);
	wl_surface_attach(pointer->cursor_surface,
			wl_cursor_image_get_buffer(pointer->cursor_image), 0, 0);
	wl_pointer_set_cursor(pointer->pointer, pointer->serial,
			pointer->cursor_surface,
			pointer->cursor_image->hotspot_x / scale,
			pointer->cursor_image->hotspot_y / scale);
	wl_surface_damage_buffer(pointer->cursor_surface, 0, 0,
			INT32_MAX, INT32_MAX);
	wl_surface_commit(pointer->cursor_surface);
}

static void wl_pointer_enter(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface,
		wl_fixed_t surface_x, wl_fixed_t surface_y) {
	struct swaybar_seat *seat = data;
	struct swaybar_pointer *pointer = &seat->pointer;
	seat->pointer.x = wl_fixed_to_double(surface_x);
	seat->pointer.y = wl_fixed_to_double(surface_y);
	pointer->serial = serial;
	struct swaybar_output *output;
	wl_list_for_each(output, &seat->bar->outputs, link) {
		if (output->surface == surface) {
			pointer->current = output;
			break;
		}
	}
	update_cursor(seat);
}

static void wl_pointer_leave(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, struct wl_surface *surface) {
	struct swaybar_seat *seat = data;
	seat->pointer.current = NULL;

	// Cancel any ongoing gesture if pointer leaves bar
	if(!gesture_tracker_check(&seat->gestures, GESTURE_TYPE_NONE)) {
		gesture_tracker_cancel(&seat->gestures);
	}
}

static void wl_pointer_motion(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
	struct swaybar_seat *seat = data;
	seat->pointer.x = wl_fixed_to_double(surface_x);
	seat->pointer.y = wl_fixed_to_double(surface_y);
}

static bool check_bindings(struct swaybar *bar, uint32_t button,
		uint32_t state) {
	bool released = state == WL_POINTER_BUTTON_STATE_RELEASED;
	for (int i = 0; i < bar->config->bindings->length; i++) {
		struct swaybar_binding *binding = bar->config->bindings->items[i];
		if (binding->button == button && binding->release == released) {
			ipc_execute_binding(bar, binding);
			return true;
		}
	}
	return false;
}

static bool process_hotspots(struct swaybar_output *output,
		double x, double y, uint32_t button) {
	struct swaybar_hotspot *hotspot;
	wl_list_for_each(hotspot, &output->hotspots, link) {
		if (x >= hotspot->x && y >= hotspot->y
				&& x < hotspot->x + hotspot->width
				&& y < hotspot->y + hotspot->height) {
			if (HOTSPOT_IGNORE == hotspot->callback(output, hotspot, x, y,
					button, hotspot->data)) {
				return true;
			}
		}
	}

	return false;
}

static void wl_pointer_button(void *data, struct wl_pointer *wl_pointer,
		uint32_t serial, uint32_t time, uint32_t button, uint32_t state) {
	struct swaybar_seat *seat = data;
	struct swaybar_pointer *pointer = &seat->pointer;
	struct swaybar_output *output = pointer->current;
	if (!sway_assert(output, "button with no active output")) {
		return;
	}

	if (check_bindings(seat->bar, button, state)) {
		return;
	}

	if (state != WL_POINTER_BUTTON_STATE_PRESSED) {
		return;
	}
	process_hotspots(output, pointer->x, pointer->y, button);
}

static void workspace_next(struct swaybar *bar, struct swaybar_output *output,
		bool left) {
	struct swaybar_config *config = bar->config;
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
	if (left) {
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

		// Since we're asking Sway to switch to 'new', it should become visible.
		// Marking it visible right now allows calling workspace_next in a loop.
		new->visible = true;
		active->visible = false;
	}
}

static void process_discrete_scroll(struct swaybar_seat *seat,
		struct swaybar_output *output, struct swaybar_pointer *pointer,
		uint32_t axis, wl_fixed_t value) {
	// If there is a button press binding, execute it, skip default behavior,
	// and check button release bindings
	uint32_t button = wl_axis_to_button(axis, value);
	if (check_bindings(seat->bar, button, WL_POINTER_BUTTON_STATE_PRESSED)) {
		check_bindings(seat->bar, button, WL_POINTER_BUTTON_STATE_RELEASED);
		return;
	}

	if (process_hotspots(output, pointer->x, pointer->y, button)) {
		return;
	}

	struct swaybar_config *config = seat->bar->config;
	double amt = wl_fixed_to_double(value);
	if (amt == 0.0 || !config->workspace_buttons) {
		check_bindings(seat->bar, button, WL_POINTER_BUTTON_STATE_RELEASED);
		return;
	}

	if (!sway_assert(!wl_list_empty(&output->workspaces), "axis with no workspaces")) {
		return;
	}

	workspace_next(seat->bar, output, amt < 0.0);

	// Check button release bindings
	check_bindings(seat->bar, button, WL_POINTER_BUTTON_STATE_RELEASED);
}

static void process_continuous_scroll(struct swaybar_seat *seat,
		struct swaybar_output *output, struct swaybar_pointer *pointer,
		uint32_t axis) {
	while (abs(seat->axis[axis].value) > SWAY_CONTINUOUS_SCROLL_THRESHOLD) {
		if (seat->axis[axis].value > 0) {
			process_discrete_scroll(seat, output, pointer, axis,
				SWAY_CONTINUOUS_SCROLL_THRESHOLD);
			seat->axis[axis].value -= SWAY_CONTINUOUS_SCROLL_THRESHOLD;
		} else {
			process_discrete_scroll(seat, output, pointer, axis,
				-SWAY_CONTINUOUS_SCROLL_THRESHOLD);
			seat->axis[axis].value += SWAY_CONTINUOUS_SCROLL_THRESHOLD;
		}
	}
}

static void wl_pointer_axis(void *data, struct wl_pointer *wl_pointer,
		uint32_t time, uint32_t axis, wl_fixed_t value) {
	struct swaybar_seat *seat = data;
	struct swaybar_pointer *pointer = &seat->pointer;
	struct swaybar_output *output = pointer->current;
	if (!sway_assert(output, "axis with no active output")) {
		return;
	}

	if (!sway_assert(axis < 2, "axis out of range")) {
		return;
	}

	// If there's a while since the last scroll event,
	// set 'value' to zero as if to reset the "virtual scroll wheel"
	if (seat->axis[axis].discrete_steps == 0 &&
			time - seat->axis[axis].update_time > SWAY_CONTINUOUS_SCROLL_TIMEOUT) {
		seat->axis[axis].value = 0;
	}

	seat->axis[axis].value += value;
	seat->axis[axis].update_time = time;
}

static void wl_pointer_frame(void *data, struct wl_pointer *wl_pointer) {
	struct swaybar_seat *seat = data;
	struct swaybar_pointer *pointer = &seat->pointer;
	struct swaybar_output *output = pointer->current;

	if (output == NULL) {
		return;
	}

	for (uint32_t axis = 0; axis < 2; ++axis) {
		if (seat->axis[axis].discrete_steps) {
			for (uint32_t step = 0; step < seat->axis[axis].discrete_steps; ++step) {
				// Honestly, it would probabyl make sense to pass in
				// 'seat->axis[axis].value / seat->axis[axi].discrete_steps' here,
				// but it's only used to check whether it's positive or negative
				// so I don't think it's worth the risk of rounding errors.
				process_discrete_scroll(seat, output, pointer, axis,
					seat->axis[axis].value);
			}

			seat->axis[axis].value = 0;
			seat->axis[axis].discrete_steps = 0;
		} else {
			process_continuous_scroll(seat, output, pointer, axis);
		}
	}
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
	struct swaybar_seat *seat = data;
	if (!sway_assert(axis < 2, "axis out of range")) {
		return;
	}

	seat->axis[axis].discrete_steps += abs(discrete);
}

static const struct wl_pointer_listener pointer_listener = {
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

static struct touch_slot *get_touch_slot(struct swaybar_touch *touch, int32_t id) {
	ssize_t next = -1;
	for (size_t i = 0; i < sizeof(touch->slots) / sizeof(*touch->slots); ++i) {
		if (touch->slots[i].id == id) {
			return &touch->slots[i];
		}
		if (next == -1 && !touch->slots[i].output) {
			next = i;
		}
	}
	if (next == -1) {
		sway_log(SWAY_ERROR, "Ran out of touch slots");
		return NULL;
	}
	return &touch->slots[next];
}

static void wl_touch_down(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time, struct wl_surface *surface,
		int32_t id, wl_fixed_t _x, wl_fixed_t _y) {
	struct swaybar_seat *seat = data;
	struct swaybar_output *_output = NULL, *output = NULL;
	wl_list_for_each(_output, &seat->bar->outputs, link) {
		if (_output->surface == surface) {
			output = _output;
			break;
		}
	}
	if (!output) {
		sway_log(SWAY_DEBUG, "Got touch event for unknown surface");
		return;
	}
	struct touch_slot *slot = get_touch_slot(&seat->touch, id);
	if (!slot) {
		return;
	}
	slot->id = id;
	slot->output = output;
	slot->x = slot->start_x = wl_fixed_to_double(_x);
	slot->y = slot->start_y = wl_fixed_to_double(_y);
	slot->time = time;
}

static void wl_touch_up(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time, int32_t id) {
	struct swaybar_seat *seat = data;
	struct touch_slot *slot = get_touch_slot(&seat->touch, id);
	if (!slot) {
		return;
	}
	if (time - slot->time < 500) {
		// Tap, treat it like a pointer click
		process_hotspots(slot->output, slot->x, slot->y, BTN_LEFT);
	}
	slot->output = NULL;
}

static void wl_touch_motion(void *data, struct wl_touch *wl_touch,
		uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y) {
	struct swaybar_seat *seat = data;
	struct touch_slot *slot = get_touch_slot(&seat->touch, id);
	if (!slot) {
		return;
	}
	int prev_progress = (int)((slot->x - slot->start_x)
			/ slot->output->width * 100);
	slot->x = wl_fixed_to_double(x);
	slot->y = wl_fixed_to_double(y);
	// "progress" is a measure from 0..100 representing the fraction of the
	// output the touch gesture has travelled, positive when moving to the right
	// and negative when moving to the left.
	int progress = (int)((slot->x - slot->start_x)
			/ slot->output->width * 100);
	if (abs(progress) / 20 != abs(prev_progress) / 20) {
		workspace_next(seat->bar, slot->output, progress - prev_progress < 0);
	}
}

static void wl_touch_frame(void *data, struct wl_touch *wl_touch) {
	// Who cares
}

static void wl_touch_cancel(void *data, struct wl_touch *wl_touch) {
	struct swaybar_seat *seat = data;
	struct swaybar_touch *touch = &seat->touch;
	for (size_t i = 0; i < sizeof(touch->slots) / sizeof(*touch->slots); ++i) {
		touch->slots[i].output = NULL;
	}
}

static void wl_touch_shape(void *data, struct wl_touch *wl_touch, int32_t id,
		wl_fixed_t major, wl_fixed_t minor) {
	// Who cares
}

static void wl_touch_orientation(void *data, struct wl_touch *wl_touch,
		int32_t id, wl_fixed_t orientation) {
	// Who cares
}

static const struct wl_touch_listener touch_listener = {
	.down = wl_touch_down,
	.up = wl_touch_up,
	.motion = wl_touch_motion,
	.frame = wl_touch_frame,
	.cancel = wl_touch_cancel,
	.shape = wl_touch_shape,
	.orientation = wl_touch_orientation,
};


static bool gesture_on_bar(struct swaybar_seat *seat,
		struct wl_surface *surface) {
	struct swaybar_output *output;

	wl_list_for_each(output, &seat->bar->outputs, link) {
		if (output->surface == surface) {
			return true;
		}
	}

	return false;
}

// Check if bar has any binding for gesture
static bool bar_gestures_check(struct swaybar *bar,
		enum gesture_type type, uint8_t fingers) {
	for (int i = 0; i < bar->config->gestures->length; i++) {
		struct swaybar_gesture *binding = bar->config->gestures->items[i];
		if (gesture_check(&binding->gesture, type, fingers)) {
			return true;
		}
	}

	return false;
}

// Check bar gesture binding for match
static struct swaybar_gesture *bar_gestures_match(struct swaybar *bar,
		struct gesture *gesture) {
	struct swaybar_gesture *current = NULL;

	for (int i = 0; i < bar->config->gestures->length; i++) {
		struct swaybar_gesture *binding = bar->config->gestures->items[i];
		if (gesture_match(&binding->gesture, gesture, false)) {
			if (current &&
					gesture_compare(&current->gesture, &binding->gesture) < 0) {
				continue;
			}
			current = binding;
		}
	}

	return current;
}

static void gesture_begin(struct swaybar_seat* seat, struct wl_surface *surface,
		enum gesture_type type, uint8_t fingers) {
	// We only want to respond to holds on a bar surface
	if (!gesture_on_bar(seat, surface)) {
		return;
	}

	// Ensure there is a binding for this gesture
	if (!bar_gestures_check(seat->bar, type, fingers)) {
		sway_log(SWAY_DEBUG, "Ignore tracking gesture without binding: %s:%u:any",
			gesture_type_string(type), fingers);
		return;
	}

	gesture_tracker_begin(&seat->gestures, type, fingers);
}

static void gesture_end(struct swaybar_seat* seat, enum gesture_type type,
		bool cancelled) {

	if (!gesture_tracker_check(&seat->gestures, type)) {
		return;
	}

	if (cancelled) {
		gesture_tracker_cancel(&seat->gestures);
		return;
	}

	struct gesture *gesture = gesture_tracker_end(&seat->gestures);
	struct swaybar_gesture* binding = bar_gestures_match(seat->bar, gesture);

	if (binding) {
		ipc_execute_gesture(seat->bar, binding);
	}
}

static void hold_begin(void *data, struct zwp_pointer_gesture_hold_v1 *wl_hold,
		uint32_t serial, uint32_t time,struct wl_surface *surface,
		uint32_t fingers) {
	struct swaybar_seat *seat = data;
	gesture_begin(seat, surface, GESTURE_TYPE_HOLD, fingers);
}

static void hold_end(void *data,
		struct zwp_pointer_gesture_hold_v1 *wl_hold,
		uint32_t serial, uint32_t time, int32_t cancelled) {
	struct swaybar_seat *seat = data;
	gesture_end(seat, GESTURE_TYPE_HOLD, cancelled);
}

static const struct zwp_pointer_gesture_hold_v1_listener hold_listener = {
	.begin = hold_begin,
	.end = hold_end,
};

static void pinch_begin(void *data,
		struct zwp_pointer_gesture_pinch_v1 *wl_pinch, uint32_t serial,
		uint32_t time, struct wl_surface *surface, uint32_t fingers) {
	struct swaybar_seat *seat = data;
	gesture_begin(seat, surface, GESTURE_TYPE_PINCH, fingers);
}

static void pinch_update(void *data,
		struct zwp_pointer_gesture_pinch_v1 *wl_pinch,
		uint32_t time, wl_fixed_t dx, wl_fixed_t dy,
		wl_fixed_t scale, wl_fixed_t rotation) {
	struct swaybar_seat *seat = data;
	if (gesture_tracker_check(&seat->gestures, GESTURE_TYPE_PINCH)) {
		gesture_tracker_update(&seat->gestures,
			wl_fixed_to_double(dx), wl_fixed_to_double(dy),
			wl_fixed_to_double(scale),
			wl_fixed_to_double(rotation));
	}
}

static void pinch_end(void *data,
		struct zwp_pointer_gesture_pinch_v1 *wl_pinch,
		uint32_t serial, uint32_t time, int32_t cancelled) {
	struct swaybar_seat *seat = data;
	gesture_end(seat, GESTURE_TYPE_PINCH, cancelled);
}

static const struct zwp_pointer_gesture_pinch_v1_listener pinch_listener = {
	.begin = pinch_begin,
	.update = pinch_update,
	.end = pinch_end,
};

static void swipe_begin(void *data,
		struct zwp_pointer_gesture_swipe_v1 *wl_swipe,
		uint32_t serial, uint32_t time,
		struct wl_surface *surface, uint32_t fingers) {
	struct swaybar_seat *seat = data;
	gesture_begin(seat, surface, GESTURE_TYPE_SWIPE, fingers);
}

static void swipe_update(void *data,
		struct zwp_pointer_gesture_swipe_v1 *wl_swipe,
		uint32_t time, wl_fixed_t dx, wl_fixed_t dy) {
	struct swaybar_seat *seat = data;
	if (gesture_tracker_check(&seat->gestures, GESTURE_TYPE_SWIPE)) {
		gesture_tracker_update(&seat->gestures,
			wl_fixed_to_double(dx), wl_fixed_to_double(dy),
			NAN, NAN);
	}
}

static void swipe_end(void *data,
		struct zwp_pointer_gesture_swipe_v1 *zwp_pointer_gesture_swipe_v1,
		uint32_t serial, uint32_t time, int32_t cancelled) {
	struct swaybar_seat *seat = data;
	gesture_end(seat, GESTURE_TYPE_SWIPE, cancelled);
}

static const struct zwp_pointer_gesture_swipe_v1_listener swipe_listener = {
	.begin = swipe_begin,
	.update = swipe_update,
	.end = swipe_end,
};


static void seat_handle_capabilities(void *data, struct wl_seat *wl_seat,
		enum wl_seat_capability caps) {
	struct swaybar_seat *seat = data;

	bool have_pointer = caps & WL_SEAT_CAPABILITY_POINTER;
	bool have_touch = caps & WL_SEAT_CAPABILITY_TOUCH;

	if (!have_pointer && seat->pointer.pointer != NULL) {
		wl_pointer_release(seat->pointer.pointer);
		seat->pointer.pointer = NULL;
	} else if (have_pointer && seat->pointer.pointer == NULL) {
		seat->pointer.pointer = wl_seat_get_pointer(wl_seat);
		if (seat->bar->running && !seat->pointer.cursor_surface) {
			seat->pointer.cursor_surface =
				wl_compositor_create_surface(seat->bar->compositor);
			assert(seat->pointer.cursor_surface);
		}
		wl_pointer_add_listener(seat->pointer.pointer, &pointer_listener, seat);

		seat->hold = zwp_pointer_gestures_v1_get_hold_gesture(
			seat->bar->pointer_gestures, seat->pointer.pointer);
		zwp_pointer_gesture_hold_v1_add_listener(seat->hold,
			&hold_listener, seat);

		seat->pinch = zwp_pointer_gestures_v1_get_pinch_gesture(
			seat->bar->pointer_gestures, seat->pointer.pointer);
		zwp_pointer_gesture_pinch_v1_add_listener(seat->pinch,
			&pinch_listener, seat);

		seat->swipe = zwp_pointer_gestures_v1_get_swipe_gesture(
			seat->bar->pointer_gestures, seat->pointer.pointer);
		zwp_pointer_gesture_swipe_v1_add_listener(seat->swipe,
			&swipe_listener, seat);
	}
	if (!have_touch && seat->touch.touch != NULL) {
		wl_touch_release(seat->touch.touch);
		seat->touch.touch = NULL;
	} else if (have_touch && seat->touch.touch == NULL) {
		seat->touch.touch = wl_seat_get_touch(wl_seat);
		wl_touch_add_listener(seat->touch.touch, &touch_listener, seat);
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

void swaybar_seat_free(struct swaybar_seat *seat) {
	if (!seat) {
		return;
	}
	if (seat->pointer.pointer != NULL) {
		wl_pointer_release(seat->pointer.pointer);
	}
	if (seat->pointer.cursor_theme != NULL) {
		wl_cursor_theme_destroy(seat->pointer.cursor_theme);
	}
	if (seat->pointer.cursor_surface != NULL) {
		wl_surface_destroy(seat->pointer.cursor_surface);
	}
	if (seat->touch.touch != NULL) {
		wl_touch_release(seat->touch.touch);
	}
	if (seat->hold != NULL) {
		zwp_pointer_gesture_hold_v1_destroy(seat->hold);
	}
	if (seat->pinch != NULL) {
		zwp_pointer_gesture_pinch_v1_destroy(seat->pinch);
	}
	if (seat->swipe != NULL) {
		zwp_pointer_gesture_swipe_v1_destroy(seat->swipe);
	}
	wl_seat_destroy(seat->wl_seat);
	wl_list_remove(&seat->link);
	free(seat);
}
