#ifndef _SWAY_CLIENT_REGISTRY_H
#define _SWAY_CLIENT_REGISTRY_H

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include "wayland-desktop-shell-client-protocol.h"
#include "wayland-swaylock-client-protocol.h"
#include "list.h"

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

struct output_state {
        struct wl_output *output;
        uint32_t flags;
        uint32_t width, height;
};

struct xkb {
        struct xkb_state *state;
        struct xkb_context *context;
        struct xkb_keymap *keymap;
        xkb_mod_mask_t masks[MASK_LAST];
};

struct input {
	struct xkb xkb;

	xkb_keysym_t sym;
	uint32_t code;
	uint32_t last_code;
	uint32_t modifiers;

	void (*notify)(enum wl_keyboard_key_state state, xkb_keysym_t sym, uint32_t code);
};

struct registry {
        struct wl_compositor *compositor;
        struct wl_display *display;
        struct wl_pointer *pointer;
        struct wl_keyboard *keyboard;
        struct wl_seat *seat;
        struct wl_shell *shell;
        struct wl_shm *shm;
        struct desktop_shell *desktop_shell;
        struct lock *swaylock;
        struct input *input;
        list_t *outputs;
};

struct registry *registry_poll(void);
void registry_teardown(struct registry *registry);

#endif
