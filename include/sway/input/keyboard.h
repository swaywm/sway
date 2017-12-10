#include "sway/input/seat.h"

struct sway_keyboard {
	struct sway_seat *seat;
	struct wlr_input_device *device;
	struct wl_list link; // sway_seat::keyboards

	struct wl_listener keyboard_key;
	struct wl_listener keyboard_modifiers;
};

struct sway_keyboard *sway_keyboard_create(struct sway_seat *seat,
		struct wlr_input_device *device);
