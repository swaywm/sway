#ifndef _SWAYLOCK_SEAT_H
#define _SWAYLOCK_SEAT_H
#include <xkbcommon/xkbcommon.h>

struct swaylock_xkb {
	bool caps_lock;
	bool control;
	struct xkb_state *state;
	struct xkb_context *context;
	struct xkb_keymap *keymap;
};

extern const struct wl_seat_listener seat_listener;

#endif
