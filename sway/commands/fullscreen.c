#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/layout.h"
#include "util.h"

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
	bool wants_fullscreen = !view->is_fullscreen;

	if (argc) {
		wants_fullscreen = parse_boolean(argv[0], view->is_fullscreen);
	}

	view_set_fullscreen(view, wants_fullscreen);

	struct sway_container *workspace = container_parent(container, C_WORKSPACE);
	arrange_windows(workspace->parent);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
