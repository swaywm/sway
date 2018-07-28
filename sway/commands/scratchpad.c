#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/scratchpad.h"
#include "sway/tree/container.h"

struct cmd_results *cmd_scratchpad(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "scratchpad", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (strcmp(argv[0], "show") != 0) {
		return cmd_results_new(CMD_INVALID, "scratchpad",
				"Expected 'scratchpad show'");
	}
	if (!root_container.sway_root->scratchpad->length) {
		return cmd_results_new(CMD_INVALID, "scratchpad",
				"Scratchpad is empty");
	}

	if (config->handler_context.using_criteria) {
		struct sway_container *con = config->handler_context.current_container;

		// If the container is in a floating split container,
		// operate on the split container instead of the child.
		if (container_is_floating_or_child(con)) {
			while (con->parent->layout != L_FLOATING) {
				con = con->parent;
			}
		}

		// If using criteria, this command is executed for every container which
		// matches the criteria. If this container isn't in the scratchpad,
		// we'll just silently return a success.
		if (!con->scratchpad) {
			return cmd_results_new(CMD_SUCCESS, NULL, NULL);
		}
		scratchpad_toggle_container(con);
	} else {
		scratchpad_toggle_auto();
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
