#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <wayland-util.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_hide_cursor(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "hide_cursor", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	char *end;
	config->hide_cursor_timeout = strtol(argv[0], &end, 10);
	if (*end) {
		config->hide_cursor_timeout = 0;
		return cmd_results_new(CMD_INVALID, "hide_cursor",
				"Expected an integer timeout.");
	}
	if (config->hide_cursor_timeout < 100 && config->hide_cursor_timeout != 0) {
		config->hide_cursor_timeout = 100;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
