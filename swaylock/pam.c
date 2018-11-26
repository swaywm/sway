#define _POSIX_C_SOURCE 200809L
#include <pwd.h>
#include <security/pam_appl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "swaylock/swaylock.h"

void initialize_pw_backend(void) {
	// TODO: only call pam_start once. keep the same handle the whole time
}

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

bool attempt_password(struct swaylock_password *pw) {
	struct passwd *passwd = getpwuid(getuid());
	char *username = passwd->pw_name;
	const struct pam_conv local_conversation = {
		function_conversation, pw
	};
	pam_handle_t *local_auth_handle = NULL;
	int pam_err;
	if ((pam_err = pam_start("swaylock", username,
					&local_conversation, &local_auth_handle)) != PAM_SUCCESS) {
		wlr_log(WLR_ERROR, "PAM returned error %d", pam_err);
	}
	if ((pam_err = pam_authenticate(local_auth_handle, 0)) != PAM_SUCCESS) {
		wlr_log(WLR_ERROR, "pam_authenticate failed");
		goto fail;
	}
	// TODO: only call pam_end once we succeed at authing. refresh tokens beforehand
	if ((pam_err = pam_end(local_auth_handle, pam_err)) != PAM_SUCCESS) {
		wlr_log(WLR_ERROR, "pam_end failed");
		goto fail;
	}
	clear_password_buffer(pw);
	return true;
fail:
	clear_password_buffer(pw);
	return false;
}
