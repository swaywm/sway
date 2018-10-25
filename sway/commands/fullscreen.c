#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "util.h"

struct cmd_results *cmd_fullscreen(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "fullscreen", EXPECTED_AT_MOST, 1))) {
		return error;
	}
	if (!root->outputs->length) {
		return cmd_results_new(CMD_INVALID, "fullscreen",
				"Can't run this command while there's no outputs connected.");
	}
	struct sway_node *node = config->handler_context.node;
	struct sway_container *container = config->handler_context.container;
	struct sway_workspace *workspace = config->handler_context.workspace;
	if (node->type == N_WORKSPACE && workspace->tiling->length == 0) {
		return cmd_results_new(CMD_INVALID, "fullscreen",
				"Can't fullscreen an empty workspace");
	}
	if (node->type == N_WORKSPACE) {
		// Wrap the workspace's children in a container so we can fullscreen it
		container = workspace_wrap_children(workspace);
		workspace->layout = L_HORIZ;
		seat_set_focus_container(config->handler_context.seat, container);
	}
	bool enable = !container->is_fullscreen;

	if (argc) {
		enable = parse_boolean(argv[0], container->is_fullscreen);
	}

	container_set_fullscreen(container, enable);
	arrange_workspace(workspace);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
