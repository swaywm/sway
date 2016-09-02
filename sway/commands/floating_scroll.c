#include <string.h>
#include "commands.h"
#include "log.h"
#include "stringop.h"

struct cmd_results *cmd_floating_scroll(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "floating_scroll", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	if (!strcasecmp("up", argv[0])) {
		free(config->floating_scroll_up_cmd);
		if (argc < 2) {
			config->floating_scroll_up_cmd = strdup("");
		} else {
			config->floating_scroll_up_cmd = join_args(argv + 1, argc - 1);
		}
	} else if (!strcasecmp("down", argv[0])) {
		free(config->floating_scroll_down_cmd);
		if (argc < 2) {
			config->floating_scroll_down_cmd = strdup("");
		} else {
			config->floating_scroll_down_cmd = join_args(argv + 1, argc - 1);
		}
	} else if (!strcasecmp("left", argv[0])) {
		free(config->floating_scroll_left_cmd);
		if (argc < 2) {
			config->floating_scroll_left_cmd = strdup("");
		} else {
			config->floating_scroll_left_cmd = join_args(argv + 1, argc - 1);
		}
	} else if (!strcasecmp("right", argv[0])) {
		free(config->floating_scroll_right_cmd);
		if (argc < 2) {
			config->floating_scroll_right_cmd = strdup("");
		} else {
			config->floating_scroll_right_cmd = join_args(argv + 1, argc - 1);
		}
	} else {
		error = cmd_results_new(CMD_INVALID, "floating_scroll", "Unknown command: '%s'", argv[0]);
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
