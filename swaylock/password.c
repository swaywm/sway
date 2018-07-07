#define _XOPEN_SOURCE 500
#include <assert.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include "swaylock/swaylock.h"
#include "swaylock/seat.h"
#include "unicode.h"

static int function_conversation(int num_msg, const struct pam_message **msg,
		struct pam_response **resp, void *data) {
	struct swaylock_password *pw = data;
	/* PAM expects an array of responses, one for each message */
	struct pam_response *pam_reply = calloc(
			num_msg, sizeof(struct pam_response));
	*resp = pam_reply;
	for (int i = 0; i < num_msg; ++i) {
		switch (msg[i]->msg_style) {
		case PAM_PROMPT_ECHO_OFF:
		case PAM_PROMPT_ECHO_ON:
			pam_reply[i].resp = strdup(pw->buffer); // PAM clears and frees this
			break;
		case PAM_ERROR_MSG:
		case PAM_TEXT_INFO:
			break;
		}
	}
	return PAM_SUCCESS;
}

void clear_password_buffer(struct swaylock_password *pw) {
	// Use volatile keyword so so compiler can't optimize this out.
	volatile char *buffer = pw->buffer;
	volatile char zero = '\0';
	for (size_t i = 0; i < sizeof(buffer); ++i) {
		buffer[i] = zero;
	}
	pw->len = 0;
}

static bool attempt_password(struct swaylock_password *pw) {
	struct passwd *passwd = getpwuid(getuid());
	char *username = passwd->pw_name;
	const struct pam_conv local_conversation = {
		function_conversation, pw
	};
	pam_handle_t *local_auth_handle = NULL;
	int pam_err;
	// TODO: only call pam_start once. keep the same handle the whole time
	if ((pam_err = pam_start("swaylock", username,
					&local_conversation, &local_auth_handle)) != PAM_SUCCESS) {
		wlr_log(L_ERROR, "PAM returned error %d", pam_err);
	}
	if ((pam_err = pam_authenticate(local_auth_handle, 0)) != PAM_SUCCESS) {
		wlr_log(L_ERROR, "pam_authenticate failed");
		goto fail;
	}
	// TODO: only call pam_end once we succeed at authing. refresh tokens beforehand
	if ((pam_err = pam_end(local_auth_handle, pam_err)) != PAM_SUCCESS) {
		wlr_log(L_ERROR, "pam_end failed");
		goto fail;
	}
	clear_password_buffer(pw);
	return true;
fail:
	clear_password_buffer(pw);
	return false;
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

void swaylock_handle_key(struct swaylock_state *state,
		xkb_keysym_t keysym, uint32_t codepoint) {
	switch (keysym) {
	case XKB_KEY_KP_Enter: /* fallthrough */
	case XKB_KEY_Return:
		state->auth_state = AUTH_STATE_VALIDATING;
		damage_state(state);
		wl_display_dispatch(state->display);
		wl_display_roundtrip(state->display);
		if (attempt_password(&state->password)) {
			state->run_display = false;
			break;
		}
		state->auth_state = AUTH_STATE_INVALID;
		damage_state(state);
		break;
	case XKB_KEY_Delete:
	case XKB_KEY_BackSpace:
		if (backspace(&state->password)) {
			state->auth_state = AUTH_STATE_BACKSPACE;
		} else {
			state->auth_state = AUTH_STATE_CLEAR;
		}
		damage_state(state);
		break;
	case XKB_KEY_Escape:
		clear_password_buffer(&state->password);
		state->auth_state = AUTH_STATE_CLEAR;
		damage_state(state);
		break;
	case XKB_KEY_Caps_Lock:
		/* The state is getting active after this
		 * so we need to manually toggle it */
		state->xkb.caps_lock = !state->xkb.caps_lock;
		state->auth_state = AUTH_STATE_INPUT_NOP;
		damage_state(state);
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
		break;
	case XKB_KEY_u:
		if (state->xkb.control) {
			clear_password_buffer(&state->password);
			state->auth_state = AUTH_STATE_CLEAR;
			damage_state(state);
			break;
		}
		// fallthrough
	default:
		if (codepoint) {
			append_ch(&state->password, codepoint);
			state->auth_state = AUTH_STATE_INPUT;
			damage_state(state);
		}
		break;
	}
}
