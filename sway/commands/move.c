#include <string.h>
#include <strings.h>
#include <wlc/wlc.h>
#include "sway/commands.h"
#include "sway/container.h"
#include "sway/layout.h"
#include "sway/output.h"
#include "sway/workspace.h"
#include "list.h"

struct cmd_results *cmd_move(int argc, char **argv) {
	struct cmd_results *error = NULL;
	int move_amt = 10;

	if (config->reading) return cmd_results_new(CMD_FAILURE, "move", "Can't be used in config file.");
	if ((error = checkarg(argc, "move", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	const char* expected_syntax = "Expected 'move <left|right|up|down|next|prev|first> <[px] px>' or "
		"'move <container|window> to workspace <name>' or "
		"'move <container|window|workspace> to output <name|direction>' or "
		"'move position mouse'";
	swayc_t *view = current_container;

	if (argc == 2 || (argc == 3 && strcasecmp(argv[2], "px") == 0 )) {
		char *inv;
		move_amt = (int)strtol(argv[1], &inv, 10);
		if (*inv != '\0' && strcasecmp(inv, "px") != 0) {
			move_amt = 10;
		}
	}

	if (strcasecmp(argv[0], "left") == 0) {
		move_container(view, MOVE_LEFT, move_amt);
	} else if (strcasecmp(argv[0], "right") == 0) {
		move_container(view, MOVE_RIGHT, move_amt);
	} else if (strcasecmp(argv[0], "up") == 0) {
		move_container(view, MOVE_UP, move_amt);
	} else if (strcasecmp(argv[0], "down") == 0) {
		move_container(view, MOVE_DOWN, move_amt);
	} else if (strcasecmp(argv[0], "next") == 0) {
		move_container(view, MOVE_NEXT, move_amt);
	} else if (strcasecmp(argv[0], "prev") == 0) {
		move_container(view, MOVE_PREV, move_amt);
	} else if (strcasecmp(argv[0], "first") == 0) {
		move_container(view, MOVE_FIRST, move_amt);
	} else if (strcasecmp(argv[0], "container") == 0 || strcasecmp(argv[0], "window") == 0) {
		// "move container ...
		if ((error = checkarg(argc, "move container/window", EXPECTED_AT_LEAST, 4))) {
			return error;
		} else if (strcasecmp(argv[1], "to") == 0 && strcasecmp(argv[2], "workspace") == 0) {
			// move container to workspace x
			if (view->type == C_WORKSPACE) {
				if (!view->children || view->children->length == 0) {
					return cmd_results_new(CMD_FAILURE, "move", "Cannot move an empty workspace");
				}
				view = new_container(view, view->workspace_layout);
			} if (view->type != C_CONTAINER && view->type != C_VIEW) {
				return cmd_results_new(CMD_FAILURE, "move", "Can only move containers and views.");
			}

			const char *ws_name = argv[3];
			swayc_t *ws;
			if (argc == 5 && strcasecmp(ws_name, "number") == 0) {
				// move "container to workspace number x"
				ws_name = argv[4];
				ws = workspace_by_number(ws_name);
			} else {
				ws = workspace_by_name(ws_name);
			}

			if (ws == NULL) {
				ws = workspace_create(ws_name);
			}
			move_container_to(view, get_focused_container(ws));
		} else if (strcasecmp(argv[1], "to") == 0 && strcasecmp(argv[2], "output") == 0) {
			// move container to output x
			swayc_t *output = NULL;
			struct wlc_point abs_pos;
			get_absolute_center_position(view, &abs_pos);
			if (view->type == C_WORKSPACE) {
				if (!view->children || view->children->length == 0) {
					return cmd_results_new(CMD_FAILURE, "move", "Cannot move an empty workspace");
				}
				view = new_container(view, view->workspace_layout);
			} else if (view->type != C_CONTAINER && view->type != C_VIEW) {
				return cmd_results_new(CMD_FAILURE, "move", "Can only move containers and views.");
			} else if (!(output = output_by_name(argv[3], &abs_pos))) {
				return cmd_results_new(CMD_FAILURE, "move",
					"Can't find output with name/direction '%s' @ (%i,%i)", argv[3], abs_pos.x, abs_pos.y);
			}

			swayc_t *container = get_focused_container(output);
			if (container->is_floating) {
				move_container_to(view, container->parent);
			} else {
				move_container_to(view, container);
			}
		} else {
			return cmd_results_new(CMD_INVALID, "move", expected_syntax);
		}
	} else if (strcasecmp(argv[0], "workspace") == 0) {
		// move workspace (to output x)
		swayc_t *output = NULL;
		struct wlc_point abs_pos;
		get_absolute_center_position(view, &abs_pos);
		if ((error = checkarg(argc, "move workspace", EXPECTED_EQUAL_TO, 4))) {
			return error;
		} else if (strcasecmp(argv[1], "to") != 0 || strcasecmp(argv[2], "output") != 0) {
			return cmd_results_new(CMD_INVALID, "move", expected_syntax);
		} else if (!(output = output_by_name(argv[3], &abs_pos))) {
			return cmd_results_new(CMD_FAILURE, "move workspace",
				"Can't find output with name/direction '%s' @ (%i,%i)", argv[3], abs_pos.x, abs_pos.y);
		}
		if (view->type == C_WORKSPACE) {
			// This probably means we're moving an empty workspace, but
			// that's fine.
			move_workspace_to(view, output);
		} else {
			swayc_t *workspace = swayc_parent_by_type(view, C_WORKSPACE);
			move_workspace_to(workspace, output);
		}
	} else if (strcasecmp(argv[0], "scratchpad") == 0 || (strcasecmp(argv[0], "to") == 0 && strcasecmp(argv[1], "scratchpad") == 0)) {
		// move scratchpad ...
		if (view->type != C_CONTAINER && view->type != C_VIEW) {
			return cmd_results_new(CMD_FAILURE, "move scratchpad", "Can only move containers and views.");
		}
		swayc_t *view = current_container;
		int i;
		for (i = 0; i < scratchpad->length; i++) {
			if (scratchpad->items[i] == view) {
				hide_view_in_scratchpad(view);
				sp_view = NULL;
				return cmd_results_new(CMD_SUCCESS, NULL, NULL);
			}
		}
		list_add(scratchpad, view);
		if (!view->is_floating) {
			destroy_container(remove_child(view));
		} else {
			remove_child(view);
		}
		wlc_view_set_mask(view->handle, 0);
		arrange_windows(swayc_active_workspace(), -1, -1);
		swayc_t *focused = container_under_pointer();
		if (focused == NULL) {
			focused = swayc_active_workspace();
		}
		set_focused_container(focused);
	} else if (strcasecmp(argv[0], "position") == 0) {
		if ((error = checkarg(argc, "move workspace", EXPECTED_EQUAL_TO, 2))) {
			return error;
		}
		if (strcasecmp(argv[1], "mouse")) {
			return cmd_results_new(CMD_INVALID, "move", expected_syntax);
		}

		if (view->is_floating) {
			swayc_t *output = swayc_parent_by_type(view, C_OUTPUT);
			struct wlc_geometry g;
			wlc_view_get_visible_geometry(view->handle, &g);
			const struct wlc_size *size = wlc_output_get_resolution(output->handle);

			struct wlc_point origin;
			wlc_pointer_get_position(&origin);

			int32_t x = origin.x - g.size.w / 2;
			int32_t y = origin.y - g.size.h / 2;

			uint32_t w = size->w - g.size.w;
			uint32_t h = size->h - g.size.h;

			view->x = g.origin.x = MIN((int32_t)w, MAX(x, 0));
			view->y = g.origin.y = MIN((int32_t)h, MAX(y, 0));

			wlc_view_set_geometry(view->handle, 0, &g);
		}
	} else {
		return cmd_results_new(CMD_INVALID, "move", expected_syntax);
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
