#include <string.h>
#include "sway/commands.h"
#include "sway/container.h"
#include "sway/layout.h"

/**
 * handle "layout auto" command group
 */
static struct cmd_results *cmd_layout_auto(swayc_t *container, int argc, char **argv);

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
			if (parent->layout == L_HORIZ && (parent->workspace_layout == L_NONE
					|| parent->workspace_layout == L_HORIZ)) {
				swayc_change_layout(parent, L_VERT);
			} else {
				swayc_change_layout(parent, L_HORIZ);
			}
		} else if (strcasecmp(argv[0], "auto") == 0) {
			return cmd_layout_auto(parent, argc, argv);
		}
	}

	update_layout_geometry(parent, old_layout);
	update_geometry(parent);

	arrange_windows(parent, parent->width, parent->height);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *cmd_layout_auto(swayc_t *container, int argc, char **argv) {
	// called after checking that argv[0] is auto, so just continue parsing from there
	struct cmd_results *error = NULL;
	const char *cmd_name = "layout auto";
	const char *set_inc_cmd_name = "layout auto [master|ncol] [set|inc]";
	const char *err_msg = "Allowed arguments are <right|left|top|bot|next|prev|master|ncol>";

	bool need_layout_update = false;
	enum swayc_layouts old_layout = container->layout;
	enum swayc_layouts layout = old_layout;

	if (strcasecmp(argv[1], "left") == 0) {
		layout = L_AUTO_LEFT;
	} else if (strcasecmp(argv[1], "right") == 0) {
		layout = L_AUTO_RIGHT;
	} else if (strcasecmp(argv[1], "top") == 0) {
		layout = L_AUTO_TOP;
	} else if (strcasecmp(argv[1], "bot") == 0) {
		layout = L_AUTO_BOTTOM;
	} else if (strcasecmp(argv[1], "next") == 0) {
		if (is_auto_layout(container->layout) && container->layout < L_AUTO_LAST) {
			layout = container->layout + 1;
		} else {
			layout = L_AUTO_FIRST;
		}
	} else if (strcasecmp(argv[1], "prev") == 0) {
		if (is_auto_layout(container->layout) && container->layout > L_AUTO_FIRST) {
			layout = container->layout - 1;
		} else {
			layout = L_AUTO_LAST;
		}
	} else {
		bool is_nmaster;
		bool is_set;
		if (strcasecmp(argv[1], "master") == 0) {
			is_nmaster = true;
		} else if (strcasecmp(argv[1], "ncol") == 0) {
			is_nmaster = false;
		} else {
			return cmd_results_new(CMD_INVALID, cmd_name, "Invalid %s command. %s",
				cmd_name, err_msg);
		}
		if ((error = checkarg(argc, "auto <master|ncol>", EXPECTED_EQUAL_TO, 4))) {
			return error;
		}
		if (strcasecmp(argv[2], "set") == 0) {
			is_set = true;
		} else if (strcasecmp(argv[2], "inc") == 0) {
			is_set = false;
		} else {
			return cmd_results_new(CMD_INVALID, set_inc_cmd_name, "Invalid %s command. %s, "
					       "Argument must be on of <set|inc>",
					       set_inc_cmd_name);
		}
		char *end;
		int n = (int)strtol(argv[3], &end, 10);
		if (*end) {
			return cmd_results_new(CMD_INVALID, set_inc_cmd_name, "Invalid %s command "
					       "(argument must be an integer)", set_inc_cmd_name);
		}
		if (is_auto_layout(container->layout)) {
			int inc = 0;	/* difference between current master/ncol and requested value */
			if (is_nmaster) {
				if (is_set) {
					if (n < 0) {
						return cmd_results_new(CMD_INVALID, set_inc_cmd_name, "Invalid %s command "
								"(master must be >= 0)", set_inc_cmd_name);
					}
					inc = n - (int)container->nb_master;
				} else {	/* inc command */
					if ((int)container->nb_master + n >= 0) {
						inc = n;
					}
				}
				if (inc) {
					for (int i = container->nb_master;
					i >= 0 && i < container->children->length
						&& i != (int)container->nb_master + inc;) {
						((swayc_t *)container->children->items[i])->height = -1;
						((swayc_t *)container->children->items[i])->width = -1;
						i += inc > 0 ? 1 : -1;
					}
					container->nb_master += inc;
					need_layout_update = true;
				}
			} else {	/* ncol modification */
				if (is_set) {
					if (n <= 0) {
						return cmd_results_new(CMD_INVALID, set_inc_cmd_name, "Invalid %s command "
								"(ncol must be > 0)", set_inc_cmd_name);
					}
					inc = n - (int)container->nb_slave_groups;
				} else {	/* inc command */
					if ((int)container->nb_slave_groups + n > 0) {
						inc = n;
					}
				}
				if (inc) {
					container->nb_slave_groups += inc;
					need_layout_update = true;
				}
			}
		}
	}

	if (layout != old_layout) {
		swayc_change_layout(container, layout);
		update_layout_geometry(container, old_layout);
		need_layout_update = true;
	}
	if (need_layout_update) {
		update_geometry(container);
		arrange_windows(container, container->width, container->height);
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
