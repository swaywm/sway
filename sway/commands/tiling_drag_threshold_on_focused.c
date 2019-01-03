#include "sway/commands.h"
#include "util.h"

struct cmd_results *cmd_tiling_drag_threshold_on_focused(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "tiling_drag_threshold_on_focused", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	config->tiling_drag_threshold_on_focused = parse_boolean(argv[0], config->tiling_drag_threshold_on_focused);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
