#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "log.h"

struct cmd_results *cmd_layout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "layout", EXPECTED_MORE_THAN, 0))) {
		return error;
	}
	struct sway_container *parent = config->handler_context.current_container;

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
			struct sway_container *output = sway_container_parent(parent, C_OUTPUT);
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
