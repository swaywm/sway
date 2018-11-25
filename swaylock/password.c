#include <assert.h>
#include <errno.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include "swaylock/swaylock.h"
#include "swaylock/seat.h"
#include "loop.h"
#include "unicode.h"

void clear_password_buffer(struct swaylock_password *pw) {
	// Use volatile keyword so so compiler can't optimize this out.
	volatile char *buffer = pw->buffer;
	volatile char zero = '\0';
	for (size_t i = 0; i < sizeof(pw->buffer); ++i) {
		buffer[i] = zero;
	}
	pw->len = 0;
}

static bool backspace(struct swaylock_password *pw) {
	if (pw->len != 0) {
		pw->buffer[--pw->len] = 0;
		return true;
	}
	return false;
}

static void append_ch(struct swaylock_password *pw, uint32_t codepoint) {
	size_t utf8_size = utf8_chsize(codepoint);
	if (pw->len + utf8_size + 1 >= sizeof(pw->buffer)) {
		// TODO: Display error
		return;
	}
	utf8_encode(&pw->buffer[pw->len], codepoint);
	pw->buffer[pw->len + utf8_size] = 0;
	pw->len += utf8_size;
}

static void clear_indicator(void *data) {
	struct swaylock_state *state = data;
	state->clear_indicator_timer = NULL;
	state->auth_state = AUTH_STATE_IDLE;
	damage_state(state);
}

static void schedule_indicator_clear(struct swaylock_state *state) {
	if (state->clear_indicator_timer) {
		loop_remove_timer(state->eventloop, state->clear_indicator_timer);
	}
	state->clear_indicator_timer = loop_add_timer(
			state->eventloop, 3000, clear_indicator, state);
}

static void clear_password(void *data) {
	struct swaylock_state *state = data;
	state->clear_password_timer = NULL;
	state->auth_state = AUTH_STATE_CLEAR;
	clear_password_buffer(&state->password);
	damage_state(state);
	schedule_indicator_clear(state);
}

static void schedule_password_clear(struct swaylock_state *state) {
	if (state->clear_password_timer) {
		loop_remove_timer(state->eventloop, state->clear_password_timer);
	}
	state->clear_password_timer = loop_add_timer(
			state->eventloop, 10000, clear_password, state);
}

static void handle_preverify_timeout(void *data) {
	struct swaylock_state *state = data;
	state->verify_password_timer = NULL;
}

void swaylock_handle_key(struct swaylock_state *state,
		xkb_keysym_t keysym, uint32_t codepoint) {
	switch (keysym) {
	case XKB_KEY_KP_Enter: /* fallthrough */
	case XKB_KEY_Return:
		if (state->args.ignore_empty && state->password.len == 0) {
			break;
		}

		state->auth_state = AUTH_STATE_VALIDATING;
		damage_state(state);

		// We generally want to wait until all surfaces are showing the
		// "verifying" state before we go and verify the password, because
		// verifying it is a blocking operation. However, if the surface is on
		// an output with DPMS off then it won't update, so we set a timer.
		state->verify_password_timer = loop_add_timer(
				state->eventloop, 50, handle_preverify_timeout, state);

		while (state->run_display && state->verify_password_timer) {
			errno = 0;
			if (wl_display_flush(state->display) == -1 && errno != EAGAIN) {
				break;
			}
			loop_poll(state->eventloop);

			bool ok = 1;
			struct swaylock_surface *surface;
			wl_list_for_each(surface, &state->surfaces, link) {
				if (surface->dirty) {
					ok = 0;
				}
			}
			if (ok) {
				break;
			}
		}
		wl_display_flush(state->display);

		if (attempt_password(&state->password)) {
			state->run_display = false;
			break;
		}
		state->auth_state = AUTH_STATE_INVALID;
		damage_state(state);
		schedule_indicator_clear(state);
		break;
	case XKB_KEY_Delete:
	case XKB_KEY_BackSpace:
		if (backspace(&state->password)) {
			state->auth_state = AUTH_STATE_BACKSPACE;
		} else {
			state->auth_state = AUTH_STATE_CLEAR;
		}
		damage_state(state);
		schedule_indicator_clear(state);
		schedule_password_clear(state);
		break;
	case XKB_KEY_Escape:
		clear_password_buffer(&state->password);
		state->auth_state = AUTH_STATE_CLEAR;
		damage_state(state);
		schedule_indicator_clear(state);
		break;
	case XKB_KEY_Caps_Lock:
		/* The state is getting active after this
		 * so we need to manually toggle it */
		state->xkb.caps_lock = !state->xkb.caps_lock;
		state->auth_state = AUTH_STATE_INPUT_NOP;
		damage_state(state);
		schedule_indicator_clear(state);
		schedule_password_clear(state);
		break;
	case XKB_KEY_Shift_L:
	case XKB_KEY_Shift_R:
	case XKB_KEY_Control_L:
	case XKB_KEY_Control_R:
	case XKB_KEY_Meta_L:
	case XKB_KEY_Meta_R:
	case XKB_KEY_Alt_L:
	case XKB_KEY_Alt_R:
	case XKB_KEY_Super_L:
	case XKB_KEY_Super_R:
		state->auth_state = AUTH_STATE_INPUT_NOP;
		damage_state(state);
		schedule_indicator_clear(state);
		schedule_password_clear(state);
		break;
	case XKB_KEY_u:
		if (state->xkb.control) {
			clear_password_buffer(&state->password);
			state->auth_state = AUTH_STATE_CLEAR;
			damage_state(state);
			schedule_indicator_clear(state);
			break;
		}
		// fallthrough
	default:
		if (codepoint) {
			append_ch(&state->password, codepoint);
			state->auth_state = AUTH_STATE_INPUT;
			damage_state(state);
			schedule_indicator_clear(state);
			schedule_password_clear(state);
		}
		break;
	}
}
