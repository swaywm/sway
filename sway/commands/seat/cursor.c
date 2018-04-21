#define _XOPEN_SOURCE 700
#ifdef __linux__
#include <linux/input-event-codes.h>
#elif __FreeBSD__
#include <dev/evdev/input-event-codes.h>
#endif

#include <strings.h>
#include <wlr/types/wlr_cursor.h>
#include "sway/commands.h"
#include "sway/input/cursor.h"

static struct cmd_results *press_or_release(struct sway_cursor *cursor,
		char *action, char *button_str);

static const char *expected_syntax = "Expected 'cursor <move> <x> <y>' or "
					"'cursor <set> <x> <y>' or "
					"'curor <press|release> <left|right|1|2|3...>'";

struct cmd_results *seat_cmd_cursor(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "cursor", EXPECTED_AT_LEAST, 2))) {
		return error;
	}
	struct sway_seat *seat = config->handler_context.seat;
	if (!seat) {
		return cmd_results_new(CMD_FAILURE, "cursor", "No seat defined");
	}

	struct sway_cursor *cursor = seat->cursor;

	if (strcasecmp(argv[0], "move") == 0) {
		if (argc < 3) {
			return cmd_results_new(CMD_INVALID, "cursor", expected_syntax);
		}
		int delta_x = strtol(argv[1], NULL, 10);
		int delta_y = strtol(argv[2], NULL, 10);
		wlr_cursor_move(cursor->cursor, NULL, delta_x, delta_y);
		cursor_send_pointer_motion(cursor, 0);
	} else if (strcasecmp(argv[0], "set") == 0) {
		if (argc < 3) {
			return cmd_results_new(CMD_INVALID, "cursor", expected_syntax);
		}
		// map absolute coords (0..1,0..1) to root container coords
		float x = strtof(argv[1], NULL) / root_container.width;
		float y = strtof(argv[2], NULL) / root_container.height;
		wlr_cursor_warp_absolute(cursor->cursor, NULL, x, y);
		cursor_send_pointer_motion(cursor, 0);
	} else {
		if (argc < 2) {
			return cmd_results_new(CMD_INVALID, "cursor", expected_syntax);
		}
		if ((error = press_or_release(cursor, argv[0], argv[1]))) {
			return error;
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *press_or_release(struct sway_cursor *cursor,
		char *action, char *button_str) {
	enum wlr_button_state state;
	uint32_t button;
	if (strcasecmp(action, "press") == 0) {
		state = WLR_BUTTON_PRESSED;
	} else if (strcasecmp(action, "release") == 0) {
		state = WLR_BUTTON_RELEASED;
	} else {
		return cmd_results_new(CMD_INVALID, "cursor", expected_syntax);
	}

	if (strcasecmp(button_str, "left") == 0) {
		button = BTN_LEFT;
	} else if (strcasecmp(button_str, "right") == 0) {
		button = BTN_RIGHT;
	} else {
		button = strtol(button_str, NULL, 10);
		if (button == 0) {
			return cmd_results_new(CMD_INVALID, "cursor", expected_syntax);
		}
	}
	dispatch_cursor_button(cursor, 0, button, state);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
