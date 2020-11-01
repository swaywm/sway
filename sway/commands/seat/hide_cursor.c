#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/seat.h"
#include "sway/input/cursor.h"
#include "sway/server.h"
#include "stringop.h"
#include "util.h"

struct cmd_results *seat_cmd_hide_cursor(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "hide_cursor", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if ((error = checkarg(argc, "hide_cursor", EXPECTED_AT_MOST, 2))) {
		return error;
	}
	struct seat_config *seat_config = config->handler_context.seat_config;
	if (!seat_config) {
		return cmd_results_new(CMD_FAILURE, "No seat defined");
	}

	if (argc == 1) {
		char *end;
		int timeout = strtol(argv[0], &end, 10);
		if (*end) {
			return cmd_results_new(CMD_INVALID, "Expected an integer timeout");
		}
		if (timeout < 100 && timeout != 0) {
			timeout = 100;
		}
		seat_config->hide_cursor_timeout = timeout;
	} else {
		if (strcmp(argv[0], "when-typing") != 0) {
			return cmd_results_new(CMD_INVALID,
				"Expected 'hide_cursor <timeout>|when-typing [enable|disable]'");
		}
		seat_config->hide_cursor_when_typing = parse_boolean(argv[1], true) ?
			HIDE_WHEN_TYPING_ENABLE : HIDE_WHEN_TYPING_DISABLE;

		// Invalidate all the caches for this config
		struct sway_seat *seat = NULL;
		wl_list_for_each(seat, &server.input->seats, link) {
			seat->cursor->hide_when_typing = HIDE_WHEN_TYPING_DEFAULT;
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
