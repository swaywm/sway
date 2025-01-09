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
	if (!container) {
		return cmd_results_new(CMD_INVALID,
						 "Only valid containers can have a title_format");
	}
	char *format = join_args(argv, argc);
	if (container->title_format) {
		free(container->title_format);
	}
	container->title_format = format;
	if (container->view) {
		view_update_title(container->view, true);
	} else {
		container_update_representation(container);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
