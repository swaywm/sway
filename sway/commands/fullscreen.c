#include <wlr/types/wlr_wl_shell.h>
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/layout.h"

// fullscreen toggle|enable|disable
struct cmd_results *cmd_fullscreen(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (config->reading) return cmd_results_new(CMD_FAILURE, "fullscreen", "Can't be used in config file.");
	if (!config->active) return cmd_results_new(CMD_FAILURE, "fullscreen", "Can only be used when sway is running.");
	if ((error = checkarg(argc, "fullscreen", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	struct sway_container *container =
		config->handler_context.current_container;
	if (container->type != C_VIEW) {
		return cmd_results_new(CMD_INVALID, "fullscreen",
				"Only views can fullscreen");
	}
	struct sway_view *view = container->sway_view;
	bool wants_fullscreen;

	if (strcmp(argv[0], "enable") == 0) {
		wants_fullscreen = true;
	} else if (strcmp(argv[0], "disable") == 0) {
		wants_fullscreen = false;
	} else if (strcmp(argv[0], "toggle") == 0) {
		wants_fullscreen = !view->is_fullscreen;
	} else {
		return cmd_results_new(CMD_INVALID, "fullscreen",
				"Expected 'fullscreen <enable|disable|toggle>'");
	}

	view_set_fullscreen(view, wants_fullscreen);

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
