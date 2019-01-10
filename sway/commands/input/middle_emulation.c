#include <string.h>
#include <strings.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "util.h"

struct cmd_results *input_cmd_middle_emulation(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "middle_emulation", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	if (parse_boolean(argv[0], true)) {
		ic->middle_emulation = LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED;
	} else {
		ic->middle_emulation = LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED;
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
