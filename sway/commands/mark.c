#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

// mark foo                      Same as mark --replace foo
// mark --add foo                Add this mark to view's list
// mark --replace foo            Replace view's marks with this single one
// mark --add --toggle foo       Toggle current mark and persist other marks
// mark --replace --toggle foo   Toggle current mark and remove other marks

struct cmd_results *cmd_mark(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "mark", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct sway_container *container = config->handler_context.container;
	struct sway_workspace *workspace = config->handler_context.workspace;

	// If no container but we have a workspace (targeted via con_id criteria),
	// apply marks to the workspace
	if (!container && !workspace) {
		return cmd_results_new(CMD_INVALID,
				"Only containers and workspaces can have marks");
	}

	bool add = false, toggle = false;
	while (argc > 0 && has_prefix(*argv, "--")) {
		if (strcmp(*argv, "--add") == 0) {
			add = true;
		} else if (strcmp(*argv, "--replace") == 0) {
			add = false;
		} else if (strcmp(*argv, "--toggle") == 0) {
			toggle = true;
		} else {
			return cmd_results_new(CMD_INVALID,
					"Unrecognized argument '%s'", *argv);
		}
		++argv;
		--argc;
	}

	if (!argc) {
		return cmd_results_new(CMD_INVALID,
				"Expected '[--add|--replace] [--toggle] <identifier>'");
	}

	char *mark = join_args(argv, argc);

	if (container) {
		bool had_mark = container_has_mark(container, mark);

		if (!add) {
			// Replacing
			container_clear_marks(container);
		}

		container_find_and_unmark(mark);
		workspace_find_and_unmark(mark);

		if (!toggle || !had_mark) {
			container_add_mark(container, mark);
		}

		free(mark);
		container_update_marks(container);
		if (container->view) {
			view_execute_criteria(container->view);
		}
	} else {
		// Workspace
		bool had_mark = workspace_has_mark(workspace, mark);

		if (!add) {
			// Replacing
			workspace_clear_marks(workspace);
		}

		container_find_and_unmark(mark);
		workspace_find_and_unmark(mark);

		if (!toggle || !had_mark) {
			workspace_add_mark(workspace, mark);
		}

		free(mark);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
