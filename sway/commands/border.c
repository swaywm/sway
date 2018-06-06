#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/cursor.h"
#include "sway/input/input-manager.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"

struct cmd_results *cmd_border(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "border", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	struct sway_container *container =
		config->handler_context.current_container;
	if (container->type != C_VIEW) {
		return cmd_results_new(CMD_INVALID, "border",
				"Only views can have borders");
	}
	struct sway_view *view = container->sway_view;

	if (strcmp(argv[0], "none") == 0) {
		view->border = B_NONE;
	} else if (strcmp(argv[0], "normal") == 0) {
		view->border = B_NORMAL;
	} else if (strcmp(argv[0], "pixel") == 0) {
		view->border = B_PIXEL;
		if (argc == 2) {
			view->border_thickness = atoi(argv[1]);
		}
	} else if (strcmp(argv[0], "toggle") == 0) {
		view->border = (view->border + 1) % 3;
	} else {
		return cmd_results_new(CMD_INVALID, "border",
				"Expected 'border <none|normal|pixel|toggle>' "
				"or 'border pixel <px>'");
	}

	if (container_is_floating(view->swayc)) {
		container_set_geometry_from_floating_view(view->swayc);
	}

	arrange_and_commit(view->swayc);

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	if (seat->cursor) {
		cursor_send_pointer_motion(seat->cursor, 0, false);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
