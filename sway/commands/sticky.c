#include <string.h>
#include "commands.h"
#include "focus.h"

struct cmd_results *cmd_sticky(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "sticky", "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, "sticky", "Can only be used when sway is running.");
	if ((error = checkarg(argc, "sticky", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	char *action = argv[0];
	swayc_t *cont = get_focused_view(&root_container);
	if (strcmp(action, "toggle") == 0) {
		cont->sticky = !cont->sticky;
	} else if (strcmp(action, "enable") == 0) {
		cont->sticky = true;
	} else if (strcmp(action, "disable") == 0) {
		cont->sticky = false;
	} else {
		return cmd_results_new(CMD_FAILURE, "sticky",
				"Expected 'sticky enable|disable|toggle'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
