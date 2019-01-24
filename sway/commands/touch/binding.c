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

struct cmd_results *touch_cmd_binding(int argc, char **argv) {
	struct cmd_results *error = NULL;

	if((error = checkarg(argc, "binding", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	struct gesture_config *config = get_gesture_config(argv[1]);

	if (!config) {
		return cmd_results_new(CMD_FAILURE, "Unable to bind gesture %s", argv[1]);
	}

	config->command = join_args(argv+2, argc);
	return cmd_results_new(CMD_SUCCESS, NULL);
	
};
