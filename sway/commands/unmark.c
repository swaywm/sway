#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/view.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

static void remove_all_marks_iterator(struct sway_container *con, void *data) {
	if (con->type == C_VIEW) {
		view_clear_marks(con->sway_view);
		view_update_marks_textures(con->sway_view);
	}
}

// unmark                  Remove all marks from all views
// unmark foo              Remove single mark from whichever view has it
// [criteria] unmark       Remove all marks from matched view
// [criteria] unmark foo   Remove single mark from matched view

struct cmd_results *cmd_unmark(int argc, char **argv) {
	// Determine the view
	struct sway_view *view = NULL;
	if (config->handler_context.using_criteria) {
		struct sway_container *container =
			config->handler_context.current_container;
		if (container->type != C_VIEW) {
			return cmd_results_new(CMD_INVALID, "unmark",
					"Only views can have marks");
		}
		view = container->sway_view;
	}

	// Determine the mark
	char *mark = NULL;
	if (argc > 0) {
		mark = join_args(argv, argc);
	}

	if (view && mark) {
		// Remove the mark from the given view
		if (view_has_mark(view, mark)) {
			view_find_and_unmark(mark);
		}
	} else if (view && !mark) {
		// Clear all marks from the given view
		view_clear_marks(view);
		view_update_marks_textures(view);
	} else if (!view && mark) {
		// Remove mark from whichever view has it
		view_find_and_unmark(mark);
	} else {
		// Remove all marks from all views
		container_for_each_descendant_dfs(&root_container,
				remove_all_marks_iterator, NULL);
	}
	free(mark);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
