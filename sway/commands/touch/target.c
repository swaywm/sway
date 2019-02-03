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

struct cmd_results *touch_cmd_target(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "target", EXPECTED_EQUAL_TO, 5))) {
		return error;
	}
	sway_log(SWAY_DEBUG, "Trying to convert");
	double x = atoi(argv[1]);
	double y = atoi(argv[2]);
	double w = atoi(argv[3]);
	double h = atoi(argv[4]);
	sway_log(SWAY_DEBUG, "Converted: %f, %f, %f, %f", x,y,w,h);

	if(!config->gesture_engine) {
		return cmd_results_new(CMD_FAILURE, "No engine exists!");
	}
	
	struct gesture_target_config *conf = get_gesture_target_config(argv[0]);

	if(!conf) {
		return cmd_results_new(CMD_FAILURE, "Could not create target: %s", argv[0]);
	}

	if(conf->target != NULL) {
		return cmd_results_new(CMD_FAILURE, "target %s already bound", argv[0]);
	}
	
	struct libtouch_target *target = libtouch_target_create(config->gesture_engine, x,y,w,h);

	if(!target) {
		return cmd_results_new(CMD_FAILURE, "Could not create target");
	}

	conf->target = target;

	
	return cmd_results_new(CMD_SUCCESS, "Created target: %s", argv[0]);
	
};
