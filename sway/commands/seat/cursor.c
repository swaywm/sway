#define _POSIX_C_SOURCE 200809L
#include <linux/input-event-codes.h>

#include <strings.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_pointer.h>
#include "sway/commands.h"
#include "sway/input/cursor.h"

static struct cmd_results *press_or_release(struct sway_cursor *cursor,
		char *action, char *button_str);

static const char expected_syntax[] = "Expected 'cursor <move> <x> <y>' or "
					"'cursor <set> <x> <y>' or "
					"'cursor <press|release> <button[1-9]|event-name-or-code>'";

static struct cmd_results *handle_command(struct sway_cursor *cursor,
		int argc, char **argv) {
	if (strcasecmp(argv[0], "move") == 0) {
		if (argc < 3) {
			return cmd_results_new(CMD_INVALID, expected_syntax);
		}
		int delta_x = strtol(argv[1], NULL, 10);
		int delta_y = strtol(argv[2], NULL, 10);
		wlr_cursor_move(cursor->cursor, NULL, delta_x, delta_y);
		cursor_rebase(cursor);
		wlr_seat_pointer_notify_frame(cursor->seat->wlr_seat);
	} else if (strcasecmp(argv[0], "set") == 0) {
		if (argc < 3) {
			return cmd_results_new(CMD_INVALID, expected_syntax);
		}
		// map absolute coords (0..1,0..1) to root container coords
		float x = strtof(argv[1], NULL) / root->width;
		float y = strtof(argv[2], NULL) / root->height;
		wlr_cursor_warp_absolute(cursor->cursor, NULL, x, y);
		cursor_rebase(cursor);
		wlr_seat_pointer_notify_frame(cursor->seat->wlr_seat);
	} else {
		if (argc < 2) {
			return cmd_results_new(CMD_INVALID, expected_syntax);
		}
		struct cmd_results *error = NULL;
		if ((error = press_or_release(cursor, argv[0], argv[1]))) {
			return error;
		}
	}

	cursor_handle_activity_from_idle_source(cursor, IDLE_SOURCE_POINTER);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *seat_cmd_cursor(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "cursor", EXPECTED_AT_LEAST, 2))) {
		return error;
	}
	struct seat_config *sc = config->handler_context.seat_config;
	if (!sc) {
		return cmd_results_new(CMD_FAILURE, "No seat defined");
	}

	if (config->reading || !config->active) {
		return cmd_results_new(CMD_DEFER, NULL);
	}

	if (strcmp(sc->name, "*") != 0) {
		struct sway_seat *seat = input_manager_get_seat(sc->name, false);
		if (!seat) {
			return cmd_results_new(CMD_FAILURE,
					"Seat %s does not exist", sc->name);
		}
		error = handle_command(seat->cursor, argc, argv);
	} else {
		struct sway_seat *seat = NULL;
		wl_list_for_each(seat, &server.input->seats, link) {
			error = handle_command(seat->cursor, argc, argv);
			if (error && error->status != CMD_SUCCESS) {
				break;
			}
		}
	}

	return error ? error : cmd_results_new(CMD_SUCCESS, NULL);
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
		return cmd_results_new(CMD_INVALID, expected_syntax);
	}

	char *message = NULL;
	button = get_mouse_button(button_str, &message);
	if (message) {
		struct cmd_results *error =
			cmd_results_new(CMD_INVALID, message);
		free(message);
		return error;
	} else if (button == SWAY_SCROLL_UP || button == SWAY_SCROLL_DOWN
			|| button == SWAY_SCROLL_LEFT || button == SWAY_SCROLL_RIGHT) {
		// Dispatch axis event
		enum wlr_axis_orientation orientation =
			(button == SWAY_SCROLL_UP || button == SWAY_SCROLL_DOWN)
			? WLR_AXIS_ORIENTATION_VERTICAL
			: WLR_AXIS_ORIENTATION_HORIZONTAL;
		double delta = (button == SWAY_SCROLL_UP || button == SWAY_SCROLL_LEFT)
			? -1 : 1;
		struct wlr_pointer_axis_event event = {
			.pointer = NULL,
			.time_msec = 0,
			.source = WLR_AXIS_SOURCE_WHEEL,
			.orientation = orientation,
			.delta = delta * 15,
			.delta_discrete = delta
		};
		dispatch_cursor_axis(cursor, &event);
		wlr_seat_pointer_notify_frame(cursor->seat->wlr_seat);
		return cmd_results_new(CMD_SUCCESS, NULL);
	} else if (!button) {
		return cmd_results_new(CMD_INVALID, "Unknown button %s", button_str);
	}
	dispatch_cursor_button(cursor, NULL, 0, button, state);
	wlr_seat_pointer_notify_frame(cursor->seat->wlr_seat);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
