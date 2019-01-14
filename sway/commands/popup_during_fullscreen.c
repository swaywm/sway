#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *cmd_popup_during_fullscreen(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "popup_during_fullscreen",
					EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (strcasecmp(argv[0], "smart") == 0) {
		config->popup_during_fullscreen = POPUP_SMART;
	} else if (strcasecmp(argv[0], "ignore") == 0) {
		config->popup_during_fullscreen = POPUP_IGNORE;
	} else if (strcasecmp(argv[0], "leave_fullscreen") == 0) {
		config->popup_during_fullscreen = POPUP_LEAVE;
	} else {
		return cmd_results_new(CMD_INVALID, "Expected "
				"'popup_during_fullscreen smart|ignore|leave_fullscreen'");
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
