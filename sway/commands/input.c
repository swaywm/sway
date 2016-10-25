#include <string.h>
#include "sway/commands.h"
#include "sway/input.h"
#include "log.h"

struct cmd_results *cmd_input(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "input", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	if (config->reading && strcmp("{", argv[1]) == 0) {
		current_input_config = new_input_config(argv[0]);
		sway_log(L_DEBUG, "entering input block: %s", current_input_config->identifier);
		return cmd_results_new(CMD_BLOCK_INPUT, NULL, NULL);
	}

	if (argc > 2) {
		int argc_new = argc-2;
		char **argv_new = argv+2;

		struct cmd_results *res;
		current_input_config = new_input_config(argv[0]);
		if (strcasecmp("accel_profile", argv[1]) == 0) {
			res = input_cmd_accel_profile(argc_new, argv_new);
		} else if (strcasecmp("click_method", argv[1]) == 0) {
			res = input_cmd_click_method(argc_new, argv_new);
		} else if (strcasecmp("drag_lock", argv[1]) == 0) {
			res = input_cmd_drag_lock(argc_new, argv_new);
		} else if (strcasecmp("dwt", argv[1]) == 0) {
			res = input_cmd_dwt(argc_new, argv_new);
		} else if (strcasecmp("events", argv[1]) == 0) {
			res = input_cmd_events(argc_new, argv_new);
		} else if (strcasecmp("left_handed", argv[1]) == 0) {
			res = input_cmd_left_handed(argc_new, argv_new);
		} else if (strcasecmp("middle_emulation", argv[1]) == 0) {
			res = input_cmd_middle_emulation(argc_new, argv_new);
		} else if (strcasecmp("natural_scroll", argv[1]) == 0) {
			res = input_cmd_natural_scroll(argc_new, argv_new);
		} else if (strcasecmp("pointer_accel", argv[1]) == 0) {
			res = input_cmd_pointer_accel(argc_new, argv_new);
		} else if (strcasecmp("scroll_method", argv[1]) == 0) {
			res = input_cmd_scroll_method(argc_new, argv_new);
		} else if (strcasecmp("tap", argv[1]) == 0) {
			res = input_cmd_tap(argc_new, argv_new);
		} else {
			res = cmd_results_new(CMD_INVALID, "input <device>", "Unknown command %s", argv[1]);
		}
		current_input_config = NULL;
		return res;
	}

	return cmd_results_new(CMD_BLOCK_INPUT, NULL, NULL);
}
