#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/output.h"
#include "list.h"
#include "log.h"
#include "stringop.h"
#include "util.h"

static void rebuild_marks_iterator(struct sway_container *con, void *data) {
	container_update_marks_textures(con);
}

struct cmd_results *cmd_show_marks(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "show_marks", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	config->show_marks = parse_boolean(argv[0], config->show_marks);

	if (config->show_marks) {
		root_for_each_container(rebuild_marks_iterator, NULL);
	}

	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		output_damage_whole(output);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
