#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include <xkbcommon/xkbcommon.h>
#include "swaylock/swaylock.h"
#include "swaylock/seat.h"
#include "unicode.h"

static void backspace(struct swaylock_password *pw) {
	if (pw->len != 0) {
		pw->buffer[--pw->len] = 0;
	}
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
			// TODO: Attempt password
			break;
		case XKB_KEY_BackSpace:
			backspace(&state->password);
			break;
		default:
			if (codepoint) {
				append_ch(&state->password, codepoint);
			}
			break;
	}
}
