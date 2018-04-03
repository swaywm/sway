#include <assert.h>
#include <pwd.h>
#include <security/pam_appl.h>
#include <stdlib.h>
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
			pam_reply[i].resp = pw->buffer;
			break;
		case PAM_ERROR_MSG:
		case PAM_TEXT_INFO:
			break;
		}
	}
	return PAM_SUCCESS;
}

static bool attempt_password(struct swaylock_password *pw) {
	struct passwd *passwd = getpwuid(getuid());
	char *username = passwd->pw_name;
	const struct pam_conv local_conversation = {
		function_conversation, pw
	};
	pam_handle_t *local_auth_handle = NULL;
	int pam_err;
	if ((pam_err = pam_start("swaylock", username,
					&local_conversation, &local_auth_handle)) != PAM_SUCCESS) {
		wlr_log(L_ERROR, "PAM returned error %d", pam_err);
	}
	if ((pam_err = pam_authenticate(local_auth_handle, 0)) != PAM_SUCCESS) {
		wlr_log(L_ERROR, "pam_authenticate failed");
		goto fail;
	}
	if ((pam_err = pam_end(local_auth_handle, pam_err)) != PAM_SUCCESS) {
		wlr_log(L_ERROR, "pam_end failed");
		goto fail;
	}
	// PAM frees this
	pw->buffer = NULL;
	pw->len = pw->size = 0;
	return true;
fail:
	// PAM frees this
	pw->buffer = NULL;
	pw->len = pw->size = 0;
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
	if (!pw->buffer) {
		pw->size = 8;
		if (!(pw->buffer = malloc(pw->size))) {
			// TODO: Display error
			return;
		}
		pw->buffer[0] = 0;
	}
	size_t utf8_size = utf8_chsize(codepoint);
	if (pw->len + utf8_size + 1 >= pw->size) {
		size_t size = pw->size * 2;
		char *buffer = realloc(pw->buffer, size);
		if (!buffer) {
			// TODO: Display error
			return;
		}
		pw->size = size;
		pw->buffer = buffer;
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
			render_frames(state);
			if (attempt_password(&state->password)) {
				exit(0);
			}
			state->auth_state = AUTH_STATE_INVALID;
			render_frames(state);
			break;
		case XKB_KEY_BackSpace:
			if (backspace(&state->password)) {
				state->auth_state = AUTH_STATE_BACKSPACE;
				render_frames(state);
			}
			break;
		default:
			if (codepoint) {
				append_ch(&state->password, codepoint);
				state->auth_state = AUTH_STATE_INPUT;
				render_frames(state);
			}
			break;
	}
	// TODO: Expire state in a few seconds
}
