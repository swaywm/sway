#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/layout.h"
#include "sway/tree/view.h"
#include "list.h"

struct cmd_results *cmd_sticky(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "sticky", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct sway_container *container =
		config->handler_context.current_container;
	if (!container_is_floating(container)) {
		return cmd_results_new(CMD_FAILURE, "sticky",
			"Can't set sticky on a tiled container");
	}

	bool wants_sticky;
	if (strcasecmp(argv[0], "enable") == 0) {
		wants_sticky = true;
	} else if (strcasecmp(argv[0], "disable") == 0) {
		wants_sticky = false;
	} else if (strcasecmp(argv[0], "toggle") == 0) {
		wants_sticky = !container->is_sticky;
	} else {
		return cmd_results_new(CMD_FAILURE, "sticky",
			"Expected 'sticky <enable|disable|toggle>'");
	}

	container->is_sticky = wants_sticky;

	if (wants_sticky) {
		// move container to focused workspace
		struct sway_container *output = container_parent(container, C_OUTPUT);
		struct sway_seat *seat = input_manager_current_seat(input_manager);
		struct sway_container *focus = seat_get_focus_inactive(seat, output);
		struct sway_container *focused_workspace = container_parent(focus, C_WORKSPACE);
		struct sway_container *current_workspace = container_parent(container, C_WORKSPACE);
		if (current_workspace != focused_workspace) {
			container_move_to(container, focused_workspace);
			arrange_windows(focused_workspace);
			if (!container_reap_empty(current_workspace)) {
				arrange_windows(current_workspace);
			}
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
