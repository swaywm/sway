#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/container.h"
#include "sway/layout.h"
#include "log.h"

struct cmd_results *cmd_layout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) {
		return cmd_results_new(CMD_FAILURE, "layout", "Can't be used in config file.");
	}
	if (!config->active) {
		return cmd_results_new(CMD_FAILURE, "layout", "Can only be used when sway is running.");
	}
	if ((error = checkarg(argc, "layout", EXPECTED_MORE_THAN, 0))) {
		return error;
	}
	swayc_t *parent = config->handler_context.current_container;
	if (!sway_assert(parent != NULL, "command called without container context")) {
		return NULL;
	}

	// TODO: floating
	/*
	if (parent->is_floating) {
		return cmd_results_new(CMD_FAILURE, "layout", "Unable to change layout of floating windows");
	}
	*/

	while (parent->type == C_VIEW) {
		parent = parent->parent;
	}

	// TODO: stacks and tabs

	if (strcasecmp(argv[0], "default") == 0) {
		swayc_change_layout(parent, parent->prev_layout);
		if (parent->layout == L_NONE) {
			swayc_t *output = swayc_parent_by_type(parent, C_OUTPUT);
			swayc_change_layout(parent, default_layout(output));
		}
	} else {
		if (parent->layout != L_TABBED && parent->layout != L_STACKED) {
			parent->prev_layout = parent->layout;
		}

		if (strcasecmp(argv[0], "splith") == 0) {
			swayc_change_layout(parent, L_HORIZ);
		} else if (strcasecmp(argv[0], "splitv") == 0) {
			swayc_change_layout(parent, L_VERT);
		} else if (strcasecmp(argv[0], "toggle") == 0 && argc == 2 && strcasecmp(argv[1], "split") == 0) {
			if (parent->layout == L_HORIZ && (parent->workspace_layout == L_NONE
					|| parent->workspace_layout == L_HORIZ)) {
				swayc_change_layout(parent, L_VERT);
			} else {
				swayc_change_layout(parent, L_HORIZ);
			}
		}
	}

	arrange_windows(parent, parent->width, parent->height);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
