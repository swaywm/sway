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
		swayc_change_layout(parent, parent->prev_layout);
		if (parent->layout == L_NONE) {
			swayc_t *output = swayc_parent_by_type(parent, C_OUTPUT);
			swayc_change_layout(parent, default_layout(output));
		}
	} else {
		if (parent->layout != L_TABBED && parent->layout != L_STACKED) {
			parent->prev_layout = parent->layout;
		}

		if (strcasecmp(argv[0], "tabbed") == 0) {
			if (parent->type != C_CONTAINER && !swayc_is_empty_workspace(parent)){
				parent = new_container(parent, L_TABBED);
			}

			swayc_change_layout(parent, L_TABBED);
		} else if (strcasecmp(argv[0], "stacking") == 0) {
			if (parent->type != C_CONTAINER && !swayc_is_empty_workspace(parent)) {
				parent = new_container(parent, L_STACKED);
			}

			swayc_change_layout(parent, L_STACKED);
		} else if (strcasecmp(argv[0], "splith") == 0) {
			swayc_change_layout(parent, L_HORIZ);
		} else if (strcasecmp(argv[0], "splitv") == 0) {
			swayc_change_layout(parent, L_VERT);
		} else if (strcasecmp(argv[0], "toggle") == 0 && argc == 2 && strcasecmp(argv[1], "split") == 0) {
			if (parent->layout == L_HORIZ && (parent->workspace_layout == L_NONE || parent->workspace_layout == L_HORIZ)) {
				swayc_change_layout(parent, L_VERT);
			} else {
				swayc_change_layout(parent, L_HORIZ);
			}
		} else if (strcasecmp(argv[0], "auto_left") == 0) {
			if (parent->type != C_CONTAINER && !swayc_is_empty_workspace(parent)){
				parent = new_container(parent, L_AUTO_LEFT);
			}
			swayc_change_layout(parent, L_AUTO_LEFT);
		} else if (strcasecmp(argv[0], "auto_right") == 0) {
			if (parent->type != C_CONTAINER && !swayc_is_empty_workspace(parent)){
				parent = new_container(parent, L_AUTO_RIGHT);
			}
			swayc_change_layout(parent, L_AUTO_RIGHT);
		} else if (strcasecmp(argv[0], "auto_top") == 0) {
			if (parent->type != C_CONTAINER && !swayc_is_empty_workspace(parent)){
				parent = new_container(parent, L_AUTO_TOP);
			}
			swayc_change_layout(parent, L_AUTO_TOP);
		} else if (strcasecmp(argv[0], "auto_bot") == 0) {
			if (parent->type != C_CONTAINER && !swayc_is_empty_workspace(parent)){
				parent = new_container(parent, L_AUTO_BOTTOM);
			}
			swayc_change_layout(parent, L_AUTO_BOTTOM);
		} else if (strcasecmp(argv[0], "incnmaster") == 0)  {
			if ((error = checkarg(argc, "layout incnmaster",
					      EXPECTED_EQUAL_TO, 2))) {
				return error;
			}
			int inc = (int) strtol(argv[1], NULL, 10);
			swayc_t *container = get_focused_view(swayc_active_workspace());
			if (container && inc &&
			    is_auto_layout(container->parent->layout) &&
			    ((int)container->parent->nb_master + inc >= 0)) {
				for (int i = container->parent->nb_master;
				     i >= 0 && i < container->parent->children->length &&
					     i != (int) container->parent->nb_master + inc;) {
					((swayc_t *) container->parent->children->items[i])->height = -1;
					((swayc_t *) container->parent->children->items[i])->width = -1;
					i += inc > 0 ? 1 : -1;
				}
				container->parent->nb_master += inc;
			}
		} else if ((strcasecmp(argv[0], "incncol") == 0) && argc ==2) {
			if ((error = checkarg(argc, "layout incncol",
					      EXPECTED_EQUAL_TO, 2))) {
				return error;
			}
			int inc = (int) strtol(argv[1], NULL, 10);
			swayc_t *container = get_focused_view(swayc_active_workspace());
			if (container && inc && is_auto_layout(container->parent->layout) &&
			    ((int)container->parent->nb_slave_groups + inc >= 1)) {
				container->parent->nb_slave_groups += inc;
			}
		}
	}

	update_layout_geometry(parent, old_layout);
	update_geometry(parent);

	arrange_windows(parent, parent->width, parent->height);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
