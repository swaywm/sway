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

struct cmd_results *touch_gesture_cmd_swipe(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if((error = checkarg(argc, "swipe", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	uint32_t direction = 0;
	if (strstr(argv[0], "west")) {
	  direction |= LIBTOUCH_MOVE_NEGATIVE_X;
	}
	if (strstr(argv[0], "east")) {
	  direction |= LIBTOUCH_MOVE_POSITIVE_X;
	}
	if (strstr(argv[0], "north")) {
	  direction |= LIBTOUCH_MOVE_POSITIVE_Y;
	}
	if (strstr(argv[0], "south")) {
	  direction |= LIBTOUCH_MOVE_NEGATIVE_Y;
	}

	sway_log(SWAY_DEBUG, "Direction: %d", direction);
	if (direction == 0) {
	  return cmd_results_new(CMD_INVALID, "Direction %s invalid", argv[0]);
	}

	struct libtouch_gesture *gesture = config->handler_context.current_gesture->gesture;

	struct libtouch_action *action = libtouch_gesture_add_move(gesture, direction);

	config->handler_context.current_gesture_action = action;

	return cmd_results_new(CMD_SUCCESS, "Created new swipe");
}
