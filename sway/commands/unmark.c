#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

static void remove_container_mark(struct sway_container *con) {
	container_clear_marks(con);
	container_update_marks(con);
}

static void remove_all_container_marks_iterator(struct sway_container *con, void *data) {
	remove_container_mark(con);
}

static void remove_all_workspace_marks_iterator(struct sway_workspace *ws, void *data) {
	workspace_clear_marks(ws);
}

// unmark                  Remove all marks from all views/workspaces
// unmark foo              Remove single mark from whichever view/workspace has it
// [criteria] unmark       Remove all marks from matched view/workspace
// [criteria] unmark foo   Remove single mark from matched view/workspace

struct cmd_results *cmd_unmark(int argc, char **argv) {
	// Determine the container or workspace
	struct sway_container *con = NULL;
	struct sway_workspace *ws = NULL;
	if (config->handler_context.node_overridden) {
		con = config->handler_context.container;
		if (!con) {
			ws = config->handler_context.workspace;
		}
	}

	// Determine the mark
	char *mark = NULL;
	if (argc > 0) {
		mark = join_args(argv, argc);
	}

	if (con && mark) {
		// Remove the mark from the given container
		if (container_has_mark(con, mark)) {
			container_find_and_unmark(mark);
		}
	} else if (con && !mark) {
		// Clear all marks from the given container
		remove_container_mark(con);
	} else if (ws && mark) {
		// Remove the mark from the given workspace
		if (workspace_has_mark(ws, mark)) {
			workspace_find_and_unmark(mark);
		}
	} else if (ws && !mark) {
		// Clear all marks from the given workspace
		workspace_clear_marks(ws);
	} else if (!con && !ws && mark) {
		// Remove mark from whichever container/workspace has it
		container_find_and_unmark(mark);
		workspace_find_and_unmark(mark);
	} else {
		// Remove all marks from all containers and workspaces
		root_for_each_container(remove_all_container_marks_iterator, NULL);
		root_for_each_workspace(remove_all_workspace_marks_iterator, NULL);
	}
	free(mark);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
