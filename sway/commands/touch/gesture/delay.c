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

struct cmd_results *touch_gesture_cmd_delay(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if((error = checkarg(argc, "delay", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	long int delay = strtol(argv[0],NULL,10);
	if(delay == 0) {
		return cmd_results_new(
			CMD_INVALID, "Invalid delay %s", argv[0]);
	}
	if(!config->handler_context.current_gesture) {
		return cmd_results_new(
			CMD_FAILURE, "No current gesture");
	}
	
	struct libtouch_gesture *gesture =
		config->handler_context.current_gesture->gesture;
	
	struct libtouch_action *action =
		libtouch_gesture_add_delay(gesture, delay);
	
	config->handler_context.current_gesture_action = action;

	return cmd_results_new(CMD_SUCCESS, NULL);
};
