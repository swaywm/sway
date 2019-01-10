#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/seat.h"
#include "stringop.h"

struct cmd_results *seat_cmd_hide_cursor(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "hide_cursor", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (!config->handler_context.seat_config) {
		return cmd_results_new(CMD_FAILURE, "No seat defined");
	}

	char *end;
	int timeout = strtol(argv[0], &end, 10);
	if (*end) {
		return cmd_results_new(CMD_INVALID, "Expected an integer timeout");
	}
	if (timeout < 100 && timeout != 0) {
		timeout = 100;
	}
	config->handler_context.seat_config->hide_cursor_timeout = timeout;

	return cmd_results_new(CMD_SUCCESS, NULL);
}
