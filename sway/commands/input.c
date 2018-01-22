#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "log.h"

struct cmd_results *cmd_input(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "input", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	if (config->reading && strcmp("{", argv[1]) == 0) {
		free_input_config(config->handler_context.input_config);
		config->handler_context.input_config = new_input_config(argv[0]);
		if (!config->handler_context.input_config) {
			return cmd_results_new(CMD_FAILURE, NULL, "Couldn't allocate config");
		}
		wlr_log(L_DEBUG, "entering input block: %s", argv[0]);
		return cmd_results_new(CMD_BLOCK_INPUT, NULL, NULL);
	}

	if ((error = checkarg(argc, "input", EXPECTED_AT_LEAST, 3))) {
		return error;
	}

	bool has_context = (config->handler_context.input_config != NULL);
	if (!has_context) {
		// caller did not give a context so create one just for this command
		config->handler_context.input_config = new_input_config(argv[0]);
		if (!config->handler_context.input_config) {
			return cmd_results_new(CMD_FAILURE, NULL, "Couldn't allocate config");
		}
	}

	int argc_new = argc-2;
	char **argv_new = argv+2;

	struct cmd_results *res;
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
	} else if (strcasecmp("xkb_layout", argv[1]) == 0) {
		res = input_cmd_xkb_layout(argc_new, argv_new);
	} else if (strcasecmp("xkb_model", argv[1]) == 0) {
		res = input_cmd_xkb_model(argc_new, argv_new);
	} else if (strcasecmp("xkb_options", argv[1]) == 0) {
		res = input_cmd_xkb_options(argc_new, argv_new);
	} else if (strcasecmp("xkb_rules", argv[1]) == 0) {
		res = input_cmd_xkb_rules(argc_new, argv_new);
	} else if (strcasecmp("xkb_variant", argv[1]) == 0) {
		res = input_cmd_xkb_variant(argc_new, argv_new);
	} else {
		res = cmd_results_new(CMD_INVALID, "input <device>", "Unknown command %s", argv[1]);
	}

	if (!has_context) {
		// clean up the context we created earlier
		free_input_config(config->handler_context.input_config);
		config->handler_context.input_config = NULL;
	}

	return res;
}
