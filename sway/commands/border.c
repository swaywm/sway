#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/cursor.h"
#include "sway/input/input-manager.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"

static void set_border(struct sway_view *view,
		enum sway_container_border new_border) {
	if (view->border == B_CSD && new_border != B_CSD) {
		view_set_csd_from_server(view, false);
	} else if (view->border != B_CSD && new_border == B_CSD) {
		view_set_csd_from_server(view, true);
	}
	view->saved_border = view->border;
	view->border = new_border;
}

struct cmd_results *cmd_border(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "border", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	struct sway_container *container = config->handler_context.container;
	if (!container->view) {
		return cmd_results_new(CMD_INVALID, "border",
				"Only views can have borders");
	}
	struct sway_view *view = container->view;

	if (strcmp(argv[0], "none") == 0) {
		set_border(view, B_NONE);
	} else if (strcmp(argv[0], "normal") == 0) {
		set_border(view, B_NORMAL);
	} else if (strcmp(argv[0], "pixel") == 0) {
		set_border(view, B_PIXEL);
	} else if (strcmp(argv[0], "csd") == 0) {
		set_border(view, B_CSD);
	} else if (strcmp(argv[0], "toggle") == 0) {
		set_border(view, (view->border + 1) % 4);
	} else {
		return cmd_results_new(CMD_INVALID, "border",
				"Expected 'border <none|normal|pixel|toggle>' "
				"or 'border pixel <px>'");
	}
	if (argc == 2) {
		view->border_thickness = atoi(argv[1]);
	}

	if (container_is_floating(view->container)) {
		container_set_geometry_from_floating_view(view->container);
	}

	arrange_container(view->container);

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	if (seat->cursor) {
		cursor_send_pointer_motion(seat->cursor, 0, false);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
