#include "log.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "sway/commands.h"

static void close_container_iterator(struct sway_container *con, void *data) {
	if (con->view) {
		view_close(con->view);
	}
}

struct cmd_results *cmd_kill(int argc, char **argv) {
	if (!root->outputs->length) {
		return cmd_results_new(CMD_INVALID,
				"Can't run this command while there's no outputs connected.");
	}
	struct sway_container *con = config->handler_context.container;
	struct sway_workspace *ws = config->handler_context.workspace;

	if (con) {
		close_container_iterator(con, NULL);
		container_for_each_child(con, close_container_iterator, NULL);
	} else {
		workspace_for_each_container(ws, close_container_iterator, NULL);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
