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

static struct cmd_handler gesture_handlers[] = {
	{ "touch", touch_gesture_cmd_touch },
};

struct cmd_results *touch_cmd_gesture(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if((error = checkarg(argc, "gesture", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	
	sway_log(SWAY_DEBUG, "entering gesture block: %s", argv[0]);
	struct gesture_config *gesture = get_gesture_config(argv[0]);

	if(!gesture) {
		return cmd_results_new(CMD_FAILURE,
				       "Could not create/find gesture config");
	}

	config->handler_context.current_gesture = gesture;

	struct cmd_handler *cmd = find_handler(argv[1], gesture_handlers, sizeof(gesture_handlers));
	if (cmd) {
		return config_subcommand(argv,argc,gesture_handlers, sizeof(gesture_handlers));
	}

		
	return cmd_results_new(CMD_FAILURE,
			       "Invalid Subcommand");
};
