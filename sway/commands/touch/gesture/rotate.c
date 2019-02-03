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

struct cmd_results *touch_gesture_cmd_rotate(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if((error = checkarg(argc, "rotate", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	uint32_t mode;
	if(strcmp(argv[0],"clockwise") == 0) {
	  mode = LIBTOUCH_ROTATE_CLOCKWISE;
	} else if (strcmp(argv[0],"counterclockwise") == 0) {
	  mode = LIBTOUCH_ROTATE_ANTICLOCKWISE;
	} else {
		return cmd_results_new(CMD_FAILURE, "rotate %s is not (anti)clockwise", argv[0]);
	}
	
	if(!config->handler_context.current_gesture) {
		return cmd_results_new(CMD_FAILURE, "No current gesture");
	} else if (!config->handler_context.current_gesture->gesture) {
		return cmd_results_new(CMD_FAILURE, "No gesture in gesture config");
	}
	struct libtouch_gesture *gesture = config->handler_context.current_gesture->gesture;
	
	struct libtouch_action *action = libtouch_gesture_add_rotate(gesture, mode);
	
	config->handler_context.current_gesture_action = action;

	return cmd_results_new(CMD_SUCCESS, "Created new rotation");
};
