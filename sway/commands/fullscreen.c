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
	if (container->type == C_WORKSPACE && container->children->length == 0) {
		return cmd_results_new(CMD_INVALID, "fullscreen",
				"Can't fullscreen an empty workspace");
	}
	if (container->type == C_WORKSPACE) {
		// Wrap the workspace's children in a container so we can fullscreen it
		struct sway_container *workspace = container;
		container = container_wrap_children(container);
		workspace->layout = L_HORIZ;
		seat_set_focus(config->handler_context.seat, container);
	}
	bool enable = !container->is_fullscreen;

	if (argc) {
		enable = parse_boolean(argv[0], container->is_fullscreen);
	}

	container_set_fullscreen(container, enable);

	struct sway_container *workspace = container_parent(container, C_WORKSPACE);
	arrange_windows(workspace->parent);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
