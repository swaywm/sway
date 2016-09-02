#include <string.h>
#include "commands.h"
#include "container.h"
#include "ipc-server.h"
#include "layout.h"
#include "list.h"
#include "log.h"

struct cmd_results *cmd_floating(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "floating", "Can't be used in config file.");
	if ((error = checkarg(argc, "floating", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	swayc_t *view = get_focused_container(&root_container);
	bool wants_floating;
	if (strcasecmp(argv[0], "enable") == 0) {
		wants_floating = true;
	} else if (strcasecmp(argv[0], "disable") == 0) {
		wants_floating = false;
	} else if (strcasecmp(argv[0], "toggle") == 0) {
		wants_floating = !view->is_floating;
	} else {
		return cmd_results_new(CMD_FAILURE, "floating",
			"Expected 'floating <enable|disable|toggle>");
	}

	// Prevent running floating commands on things like workspaces
	if (view->type != C_VIEW) {
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	}

	// Change from nonfloating to floating
	if (!view->is_floating && wants_floating) {
		// Remove view from its current location
		destroy_container(remove_child(view));

		// and move it into workspace floating
		add_floating(swayc_active_workspace(), view);
		view->x = (swayc_active_workspace()->width - view->width)/2;
		view->y = (swayc_active_workspace()->height - view->height)/2;
		if (view->desired_width != -1) {
			view->width = view->desired_width;
		}
		if (view->desired_height != -1) {
			view->height = view->desired_height;
		}
		arrange_windows(swayc_active_workspace(), -1, -1);

	} else if (view->is_floating && !wants_floating) {
		// Delete the view from the floating list and unset its is_floating flag
		remove_child(view);
		view->is_floating = false;
		// Get the properly focused container, and add in the view there
		swayc_t *focused = container_under_pointer();
		// If focused is null, it's because the currently focused container is a workspace
		if (focused == NULL || focused->is_floating) {
			focused = swayc_active_workspace();
		}
		set_focused_container(focused);

		sway_log(L_DEBUG, "Non-floating focused container is %p", focused);

		// Case of focused workspace, just create as child of it
		if (focused->type == C_WORKSPACE) {
			add_child(focused, view);
		}
		// Regular case, create as sibling of current container
		else {
			add_sibling(focused, view);
		}
		// Refocus on the view once its been put back into the layout
		view->width = view->height = 0;
		arrange_windows(swayc_active_workspace(), -1, -1);
		remove_view_from_scratchpad(view);
		ipc_event_window(view, "floating");
	}
	set_focused_container(view);
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
