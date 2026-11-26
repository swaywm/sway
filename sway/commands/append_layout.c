#include "log.h"
#include "stringop.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/tree/load_layout.h"
#include "sway/tree/workspace.h"

struct cmd_results *cmd_append_layout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "append_layout", EXPECTED_AT_LEAST, 1))) {
		return error;
	}

	if (config->reading || !config->active) {
		return cmd_results_new(CMD_DEFER, NULL);
	}

	struct sway_seat *seat = input_manager_current_seat();
	struct sway_workspace *ws = seat_get_focused_workspace(seat);
	if (!ws) {
		return cmd_results_new(CMD_FAILURE,
				"append_layout: no focused workspace");
	}

	char *path = join_args(argv, argc);
	if (!path) {
		return cmd_results_new(CMD_FAILURE, "append_layout: out of memory");
	}
	if (!expand_path(&path)) {
		free(path);
		return cmd_results_new(CMD_FAILURE,
				"append_layout: failed to expand path");
	}

	char *err = NULL;
	bool ok = load_layout_from_file(ws, path, &err);
	free(path);
	if (!ok) {
		struct cmd_results *res = cmd_results_new(CMD_FAILURE, "%s",
				err ? err : "append_layout: unknown error");
		free(err);
		return res;
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
