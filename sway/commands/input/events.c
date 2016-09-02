#include <string.h>
#include "commands.h"
#include "input.h"
#include "log.h"

struct cmd_results *input_cmd_events(int argc, char **argv) {
	sway_log(L_DEBUG, "events for device: %s", current_input_config->identifier);
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "events", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "events", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "enabled") == 0) {
		new_config->send_events = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
	} else if (strcasecmp(argv[0], "disabled") == 0) {
		new_config->send_events = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
	} else if (strcasecmp(argv[0], "disabled_on_external_mouse") == 0) {
		new_config->send_events = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
	} else {
		return cmd_results_new(CMD_INVALID, "events", "Expected 'events <enabled|disabled|disabled_on_external_mouse>'");
	}

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
