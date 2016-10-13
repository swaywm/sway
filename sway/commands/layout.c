#include <string.h>
#include "sway/commands.h"
#include "sway/container.h"
#include "sway/layout.h"

struct cmd_results *cmd_layout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "layout", "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, "layout", "Can only be used when sway is running.");
	if ((error = checkarg(argc, "layout", EXPECTED_MORE_THAN, 0))) {
		return error;
	}
	swayc_t *parent = get_focused_container(&root_container);
	if (parent->is_floating) {
		return cmd_results_new(CMD_FAILURE, "layout", "Unable to change layout of floating windows");
	}

	while (parent->type == C_VIEW) {
		parent = parent->parent;
	}

	enum swayc_layouts old_layout = parent->layout;

	if (strcasecmp(argv[0], "default") == 0) {
		parent->layout = parent->prev_layout;
		if (parent->layout == L_NONE) {
			swayc_t *output = swayc_parent_by_type(parent, C_OUTPUT);
			parent->layout = default_layout(output);
		}
	} else {
		if (parent->layout != L_TABBED && parent->layout != L_STACKED) {
			parent->prev_layout = parent->layout;
		}

		if (strcasecmp(argv[0], "tabbed") == 0) {
			if (parent->type != C_CONTAINER && !swayc_is_empty_workspace(parent)){
				parent = new_container(parent, L_TABBED);
			}

			parent->layout = L_TABBED;
		} else if (strcasecmp(argv[0], "stacking") == 0) {
			if (parent->type != C_CONTAINER && !swayc_is_empty_workspace(parent)) {
				parent = new_container(parent, L_STACKED);
			}

			parent->layout = L_STACKED;
		} else if (strcasecmp(argv[0], "splith") == 0) {
			parent->layout = L_HORIZ;
		} else if (strcasecmp(argv[0], "splitv") == 0) {
			parent->layout = L_VERT;
		} else if (strcasecmp(argv[0], "toggle") == 0 && argc == 2 && strcasecmp(argv[1], "split") == 0) {
			if (parent->layout == L_VERT) {
				parent->layout = L_HORIZ;
			} else {
				parent->layout = L_VERT;
			}
		}
	}

	update_layout_geometry(parent, old_layout);
	update_geometry(parent);

	arrange_windows(parent, parent->width, parent->height);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
