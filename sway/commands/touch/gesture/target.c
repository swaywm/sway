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

struct cmd_results *touch_gesture_cmd_target(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if((error = checkarg(argc, "target", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	struct gesture_target_config *target = get_gesture_target_config(argv[0]);

	if(!target) {
		return cmd_results_new(CMD_FAILURE, "Could not find gesture target %s", argv[0]);
	}

	if(!config->handler_context.current_gesture_action) {
		return cmd_results_new(CMD_FAILURE,
				       "No action created for target %s", argv[0]);
	     
	}

	libtouch_action_set_target(
		config->handler_context.current_gesture_action,
		target->target);

	return cmd_results_new(CMD_SUCCESS, "Bound target %s to action", argv[0]);
	

}
