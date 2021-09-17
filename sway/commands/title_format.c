#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/view.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_title_format(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "title_format", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct sway_container *container = config->handler_context.container;
	if (!container || !container->view) {
		return cmd_results_new(CMD_INVALID,
				"Only views can have a title_format");
	}
	struct sway_view *view = container->view;
	char *format = join_args(argv, argc);
	if (view->title_format) {
		free(view->title_format);
	}
	view->title_format = format;
	view_update_title(view, true);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
