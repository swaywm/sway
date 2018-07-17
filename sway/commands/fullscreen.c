#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/layout.h"

struct cmd_results *cmd_fullscreen(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "fullscreen", EXPECTED_LESS_THAN, 2))) {
		return error;
	}
	struct sway_container *container =
		config->handler_context.current_container;
	if (container->type != C_VIEW) {
		return cmd_results_new(CMD_INVALID, "fullscreen",
				"Only views can fullscreen");
	}
	struct sway_view *view = container->sway_view;
	bool wants_fullscreen;

	if (argc == 0 || strcmp(argv[0], "toggle") == 0) {
		wants_fullscreen = !view->is_fullscreen;
	} else if (strcmp(argv[0], "enable") == 0) {
		wants_fullscreen = true;
	} else if (strcmp(argv[0], "disable") == 0) {
		wants_fullscreen = false;
	} else {
		return cmd_results_new(CMD_INVALID, "fullscreen",
				"Expected 'fullscreen' or 'fullscreen <enable|disable|toggle>'");
	}

	view_set_fullscreen(view, wants_fullscreen);

	struct sway_container *workspace = container_parent(container, C_WORKSPACE);
	arrange_windows(workspace->parent);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
