#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/view.h"
#include "sway/output.h"
#include "list.h"
#include "log.h"
#include "stringop.h"

static void rebuild_marks_iterator(struct sway_container *con, void *data) {
	if (con->type == C_VIEW) {
		view_update_marks_textures(con->sway_view);
	}
}

struct cmd_results *cmd_show_marks(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "show_marks", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (strcmp(*argv, "yes") == 0) {
		config->show_marks = true;
	} else if (strcmp(*argv, "no") == 0) {
		config->show_marks = false;
	} else {
		return cmd_results_new(CMD_INVALID, "show_marks",
				"Expected 'show_marks <yes|no>'");
	}

	if (config->show_marks) {
		container_for_each_descendant_dfs(&root_container,
				rebuild_marks_iterator, NULL);
	}

	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *con = root_container.children->items[i];
		output_damage_whole(con->sway_output);
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
