#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"

struct cmd_results *seat_cmd_keep_keyboard_layout(int argc, char **argv) {
	const char *STR_LAYOUT_GLOBAL = "global",
		*STR_LAYOUT_PER_WINDOW = "per_window";
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "keep_keyboard_layout", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (!config->handler_context.seat_config) {
		return cmd_results_new(CMD_FAILURE, "No seat defined");
	}

	if (!strcmp(argv[0], STR_LAYOUT_GLOBAL)) {
		config->handler_context.seat_config->keep_keyboard_layout
			= KEYBOARD_LAYOUT_GLOBAL;
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	} else if (!strcmp(argv[0], STR_LAYOUT_PER_WINDOW)) {
		config->handler_context.seat_config->keep_keyboard_layout
			= KEYBOARD_LAYOUT_PER_WINDOW;
		return cmd_results_new(CMD_SUCCESS, NULL, NULL);
	} else {
		return cmd_results_new(CMD_FAILURE, "keep_keyboard_layout",
				"Expected 'keep_keyboard_layout global|per_window");
	}
}
