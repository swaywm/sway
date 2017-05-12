#include <string.h>
#include <strings.h>
#include <wlc/wlc.h>
#include "sway/commands.h"
#include "sway/container.h"
#include "sway/focus.h"
#include "sway/input_state.h"
#include "sway/output.h"
#include "sway/workspace.h"

struct cmd_results *cmd_focus(int argc, char **argv) {
	if (config->reading) return cmd_results_new(CMD_FAILURE, "focus", "Can't be used in config file.");
	struct cmd_results *error = NULL;
	if (argc > 0 && strcasecmp(argv[0], "output") == 0) {
		swayc_t *output = NULL;
		struct wlc_point abs_pos;
		get_absolute_center_position(get_focused_container(&root_container), &abs_pos);
		if ((error = checkarg(argc, "focus", EXPECTED_EQUAL_TO, 2))) {
			return error;
		} else if (!(output = output_by_name(argv[1], &abs_pos))) {
			return cmd_results_new(CMD_FAILURE, "focus output",
				"Can't find output with name/at direction '%s' @ (%i,%i)", argv[1], abs_pos.x, abs_pos.y);
		} else if (!workspace_switch(swayc_active_workspace_for(output))) {
			return cmd_results_new(CMD_FAILURE, "focus output",
				"Switching to workspace on output '%s' was blocked", argv[1]);
		} else if (config->mouse_warping) {
			swayc_t *focused = get_focused_view(output);
			if (focused && focused->type == C_VIEW) {
				center_pointer_on(focused);
			}
		}
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	} else if (argc == 0) {
		set_focused_container(current_container);
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	} else if ((error = checkarg(argc, "focus", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	static size_t floating_toggled_index = 0;
	static size_t tiled_toggled_index = 0;
	if (strcasecmp(argv[0], "left") == 0) {
		move_focus(MOVE_LEFT);
	} else if (strcasecmp(argv[0], "right") == 0) {
		move_focus(MOVE_RIGHT);
	} else if (strcasecmp(argv[0], "up") == 0) {
		move_focus(MOVE_UP);
	} else if (strcasecmp(argv[0], "down") == 0) {
		move_focus(MOVE_DOWN);
	} else if (strcasecmp(argv[0], "parent") == 0) {
		move_focus(MOVE_PARENT);
	} else if (strcasecmp(argv[0], "child") == 0) {
		move_focus(MOVE_CHILD);
	} else if (strcasecmp(argv[0], "next") == 0) {
		move_focus(MOVE_NEXT);
	} else if (strcasecmp(argv[0], "prev") == 0) {
		move_focus(MOVE_PREV);
	} else if (strcasecmp(argv[0], "mode_toggle") == 0) {
		swayc_t *workspace = swayc_active_workspace();
		swayc_t *focused = get_focused_view(workspace);
		if (focused->is_floating) {
			if (workspace->children->length > 0) {
				for (size_t i = 0;i < workspace->floating->length; i++) {
					swayc_t *item = *(swayc_t **)list_get(workspace->floating, i);
					if (item == focused) {
						floating_toggled_index = i;
						break;
					}
				}
				if (workspace->children->length > tiled_toggled_index) {
					swayc_t *item = *(swayc_t **)list_get(workspace->children, tiled_toggled_index);
					set_focused_container(get_focused_view(item));
				} else {
					swayc_t *item = *(swayc_t **)list_get(workspace->children, 0);
					set_focused_container(get_focused_view(item));
					tiled_toggled_index = 0;
				}
			}
		} else {
			if (workspace->floating->length > 0) {
				for (size_t i = 0;i < workspace->children->length; i++) {
					swayc_t *item = *(swayc_t **)list_get(workspace->children, i);
					if (item == focused) {
						tiled_toggled_index = i;
						break;
					}
				}
				if (workspace->floating->length > floating_toggled_index) {
					swayc_t *floating = *(swayc_t **)list_get(workspace->floating, floating_toggled_index);
					set_focused_container(get_focused_view(floating));
				} else {
					swayc_t *floating = *(swayc_t **)list_get(workspace->floating, workspace->floating->length - 1);
					set_focused_container(get_focused_view(floating));
					tiled_toggled_index = workspace->floating->length - 1;
				}
			}
		}
	} else {
		return cmd_results_new(CMD_INVALID, "focus",
				"Expected 'focus <direction|parent|child|mode_toggle>' or 'focus output <direction|name>'");
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
