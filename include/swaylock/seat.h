#ifndef _SWAYLOCK_SEAT_H
#define _SWAYLOCK_SEAT_H
#include <xkbcommon/xkbcommon.h>

enum mod_bit {
	MOD_SHIFT = 1<<0,
	MOD_CAPS = 1<<1,
	MOD_CTRL = 1<<2,
	MOD_ALT = 1<<3,
	MOD_MOD2 = 1<<4,
	MOD_MOD3 = 1<<5,
	MOD_LOGO = 1<<6,
	MOD_MOD5 = 1<<7,
};

enum mask {
	MASK_SHIFT,
	MASK_CAPS,
	MASK_CTRL,
	MASK_ALT,
	MASK_MOD2,
	MASK_MOD3,
	MASK_LOGO,
	MASK_MOD5,
	MASK_LAST
};

struct swaylock_xkb {
	uint32_t modifiers;
	bool caps_lock;
	struct xkb_state *state;
	struct xkb_context *context;
	struct xkb_keymap *keymap;
	xkb_mod_mask_t masks[MASK_LAST];
};

extern const struct wl_seat_listener seat_listener;

#endif
