#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/workspace.h"

static void scratchpad_toggle_auto(void) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focus(seat);
	struct sway_container *ws = focus->type == C_WORKSPACE ?
		focus : container_parent(focus, C_WORKSPACE);

	// If the focus is in a floating split container,
	// operate on the split container instead of the child.
	if (container_is_floating_or_child(focus)) {
		while (focus->parent->layout != L_FLOATING) {
			focus = focus->parent;
		}
	}


	// Check if the currently focused window is a scratchpad window and should
	// be hidden again.
	if (focus->scratchpad) {
		wlr_log(WLR_DEBUG, "Focus is a scratchpad window - hiding %s",
				focus->name);
		root_scratchpad_hide(focus);
		return;
	}

	// Check if there is an unfocused scratchpad window on the current workspace
	// and focus it.
	for (int i = 0; i < ws->sway_workspace->floating->children->length; ++i) {
		struct sway_container *floater =
			ws->sway_workspace->floating->children->items[i];
		if (floater->scratchpad && focus != floater) {
			wlr_log(WLR_DEBUG,
					"Focusing other scratchpad window (%s) in this workspace",
					floater->name);
			root_scratchpad_show(floater);
			return;
		}
	}

	// Check if there is a visible scratchpad window on another workspace.
	// In this case we move it to the current workspace.
	for (int i = 0; i < root_container.sway_root->scratchpad->length; ++i) {
		struct sway_container *con =
			root_container.sway_root->scratchpad->items[i];
		if (con->parent) {
			wlr_log(WLR_DEBUG,
					"Moving a visible scratchpad window (%s) to this workspace",
					con->name);
			root_scratchpad_show(con);
			return;
		}
	}

	// Take the container at the bottom of the scratchpad list
	if (!sway_assert(root_container.sway_root->scratchpad->length,
				"Scratchpad is empty")) {
		return;
	}
	struct sway_container *con = root_container.sway_root->scratchpad->items[0];
	wlr_log(WLR_DEBUG, "Showing %s from list", con->name);
	root_scratchpad_show(con);
}

static void scratchpad_toggle_container(struct sway_container *con) {
	if (!sway_assert(con->scratchpad, "Container isn't in the scratchpad")) {
		return;
	}

	// Check if it matches a currently visible scratchpad window and hide it.
	if (con->parent) {
		root_scratchpad_hide(con);
		return;
	}

	root_scratchpad_show(con);
}

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
