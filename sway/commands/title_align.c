#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"

static void arrange_title_bar_iterator(struct sway_container *con, void *data) {
	container_arrange_title_bar(con);
}

struct cmd_results *cmd_title_align(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "title_align", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (strcmp(argv[0], "left") == 0) {
		config->title_align = ALIGN_LEFT;
	} else if (strcmp(argv[0], "center") == 0) {
		config->title_align = ALIGN_CENTER;
	} else if (strcmp(argv[0], "right") == 0) {
		config->title_align = ALIGN_RIGHT;
	} else {
		return cmd_results_new(CMD_INVALID,
				"Expected 'title_align left|center|right'");
	}

	root_for_each_container(arrange_title_bar_iterator, NULL);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
