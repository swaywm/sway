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

struct swaylock_seat {
	struct swaylock_state *state;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;
};

extern const struct wl_seat_listener seat_listener;

#endif
