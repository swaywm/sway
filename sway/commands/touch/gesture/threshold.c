#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/ipc-server.h"
#include "list.h"
#include "log.h"
#include "stringop.h"
#include <libtouch.h>

struct cmd_results *touch_gesture_cmd_threshold(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if((error = checkarg(argc, "touch", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct libtouch_action *current = config->handler_context.current_gesture_action;
	if(!current) {
		return cmd_results_new(CMD_FAILURE, "No action created");
	}

	int threshold = atoi(argv[0]);
	if(threshold < 0) {
		return cmd_results_new(CMD_INVALID,
				       "Invalid threshold: %s", argv[0]);
	}
	sway_log(SWAY_DEBUG, "Set threshold %d", threshold);
	libtouch_action_set_threshold(current, threshold);
	return cmd_results_new(CMD_SUCCESS,
			       "Set threshold");
}
