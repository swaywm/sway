#ifndef _SWAY_INPUT_SWITCH_H
#define _SWAY_INPUT_SWITCH_H

#include "sway/input/seat.h"

struct sway_switch {
	struct sway_seat_device *seat_device;
	struct wlr_switch *wlr;
	enum wlr_switch_state state;
	enum wlr_switch_type type;

	struct wl_listener switch_toggle;
};

struct sway_switch *sway_switch_create(struct sway_seat *seat,
		struct sway_seat_device *device);

void sway_switch_configure(struct sway_switch *sway_switch);

void sway_switch_destroy(struct sway_switch *sway_switch);

void sway_switch_retrigger_bindings_for_all(void);

#endif
