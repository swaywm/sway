#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/view.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_unmark(int argc, char **argv) {
	if (argc == 0) {
		// Remove all marks from the current container
		struct sway_container *container =
			config->handler_context.current_container;
		if (container->type != C_VIEW) {
			return cmd_results_new(CMD_INVALID, "unmark",
					"Only views can have marks");
		}
		view_clear_marks(container->sway_view);
	} else {
		// Remove a single mark from whichever container has it
		char *mark = join_args(argv, argc);
		if (!view_find_and_unmark(mark)) {
			free(mark);
			return cmd_results_new(CMD_INVALID, "unmark",
					"No view exists with that mark");
		}
		free(mark);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
