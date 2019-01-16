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


void free_sway_gesture_binding(struct sway_gesture_binding *binding) {
	if(!binding) {
		return;
	}
	free(binding->command);
	free(binding);
}


struct cmd_results *cmd_touch(int argc, char **argv) {
	return NULL;
}

struct cmd_results *cmd_gesture(int argc, char **argv) {
	struct cmd_results *error = NULL;

	struct sway_gesture_binding *binding = calloc(1, sizeof(struct sway_gesture_binding));
	if(!binding) {
		return cmd_results_new(CMD_FAILURE, "Unable to allocate binding");
	}

	if(argc < 2) {
		free_sway_gesture_binding(binding);
		return cmd_results_new(CMD_FAILURE, "Invalid gesture command "
				       "(expected at least 2 arguments, got %d)", argc);
	}


	binding->command = join_args(argv+1, argc -1);
	//list_t *bindings = config->current_mode->gesture_bindings;
	
	

	return error;
}
