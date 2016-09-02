#include <string.h>
#include "sway/commands.h"
#include "sway/input.h"
#include "log.h"

struct cmd_results *input_cmd_click_method(int argc, char **argv) {
	sway_log(L_DEBUG, "click_method for device:  %d %s", current_input_config==NULL, current_input_config->identifier);
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "click_method", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!current_input_config) {
		return cmd_results_new(CMD_FAILURE, "click_method", "No input device defined.");
	}
	struct input_config *new_config = new_input_config(current_input_config->identifier);

	if (strcasecmp(argv[0], "none") == 0) {
		new_config->click_method = LIBINPUT_CONFIG_CLICK_METHOD_NONE;
	} else if (strcasecmp(argv[0], "button_areas") == 0) {
		new_config->click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
	} else if (strcasecmp(argv[0], "clickfinger") == 0) {
		new_config->click_method = LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
	} else {
		return cmd_results_new(CMD_INVALID, "click_method", "Expected 'click_method <none|button_areas|clickfinger'");
	}

	input_cmd_apply(new_config);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
