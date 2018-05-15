#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/view.h"
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
	struct sway_container *container =
		config->handler_context.current_container;
	if (container->type != C_VIEW) {
		return cmd_results_new(CMD_INVALID, "mark",
				"Only views can have marks");
	}
	struct sway_view *view = container->sway_view;

	bool add = false, toggle = false;
	while (argc > 0 && strncmp(*argv, "--", 2) == 0) {
		if (strcmp(*argv, "--add") == 0) {
			add = true;
		} else if (strcmp(*argv, "--replace") == 0) {
			add = false;
		} else if (strcmp(*argv, "--toggle") == 0) {
			toggle = true;
		} else {
			return cmd_results_new(CMD_INVALID, "mark",
					"Unrecognized argument '%s'", *argv);
		}
		++argv;
		--argc;
	}

	if (!argc) {
		return cmd_results_new(CMD_INVALID, "mark",
				"Expected '[--add|--replace] [--toggle] <identifier>'");
	}

	char *mark = join_args(argv, argc);
	bool had_mark = view_has_mark(view, mark);

	if (!add) {
		// Replacing
		view_clear_marks(view);
	}

	view_find_and_unmark(mark);

	if (!toggle || !had_mark) {
		list_add(view->marks, strdup(mark));
	}

	free(mark);
	view_execute_criteria(view);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
