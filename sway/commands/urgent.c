#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/layout.h"

struct cmd_results *cmd_urgent(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "urgent", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct sway_container *container =
		config->handler_context.current_container;
	if (container->type != C_VIEW) {
		return cmd_results_new(CMD_INVALID, "urgent",
				"Only views can be urgent");
	}
	struct sway_view *view = container->sway_view;

	if (strcmp(argv[0], "enable") == 0) {
		view_set_urgent(view, true);
	} else if (strcmp(argv[0], "disable") == 0) {
		view_set_urgent(view, false);
	} else if (strcmp(argv[0], "allow") == 0) {
		view->allow_request_urgent = true;
	} else if (strcmp(argv[0], "deny") == 0) {
		view->allow_request_urgent = false;
	} else {
		return cmd_results_new(CMD_INVALID, "urgent",
				"Expected 'urgent <enable|disable|allow|deny>'");
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
