#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "log.h"

struct cmd_results *cmd_debuglog(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "debuglog", EXPECTED_EQUAL_TO, 1))) {
		return error;
	} else if (strcasecmp(argv[0], "toggle") == 0) {
		if (config->reading) {
			return cmd_results_new(CMD_FAILURE, "debuglog toggle", "Can't be used in config file.");
		}
		if (toggle_debug_logging()) {
			sway_log(L_DEBUG, "Debuglog turned on.");
		}
	} else if (strcasecmp(argv[0], "on") == 0) {
		set_log_level(L_DEBUG);
		sway_log(L_DEBUG, "Debuglog turned on.");
	} else if (strcasecmp(argv[0], "off") == 0) {
		reset_log_level();
	} else {
		return cmd_results_new(CMD_FAILURE, "debuglog", "Expected 'debuglog on|off|toggle'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

