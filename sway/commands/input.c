#include <limits.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "sway/input/keyboard.h"
#include "log.h"
#include "stringop.h"

// must be in order for the bsearch
static const struct cmd_handler input_handlers[] = {
	{ "accel_profile", input_cmd_accel_profile },
	{ "calibration_matrix", input_cmd_calibration_matrix },
	{ "click_method", input_cmd_click_method },
	{ "clickfinger_button_map", input_cmd_clickfinger_button_map },
	{ "drag", input_cmd_drag },
	{ "drag_lock", input_cmd_drag_lock },
	{ "dwt", input_cmd_dwt },
	{ "dwtp", input_cmd_dwtp },
	{ "events", input_cmd_events },
	{ "left_handed", input_cmd_left_handed },
	{ "map_from_region", input_cmd_map_from_region },
	{ "map_to_output", input_cmd_map_to_output },
	{ "map_to_region", input_cmd_map_to_region },
	{ "middle_emulation", input_cmd_middle_emulation },
	{ "natural_scroll", input_cmd_natural_scroll },
	{ "pointer_accel", input_cmd_pointer_accel },
	{ "repeat_delay", input_cmd_repeat_delay },
	{ "repeat_rate", input_cmd_repeat_rate },
	{ "rotation_angle", input_cmd_rotation_angle },
	{ "scroll_button", input_cmd_scroll_button },
	{ "scroll_button_lock", input_cmd_scroll_button_lock },
	{ "scroll_factor", input_cmd_scroll_factor },
	{ "scroll_method", input_cmd_scroll_method },
	{ "tap", input_cmd_tap },
	{ "tap_button_map", input_cmd_tap_button_map },
	{ "three_finger_drag", input_cmd_three_finger_drag },
	{ "tool_mode", input_cmd_tool_mode },
	{ "xkb_capslock", input_cmd_xkb_capslock },
	{ "xkb_file", input_cmd_xkb_file },
	{ "xkb_layout", input_cmd_xkb_layout },
	{ "xkb_model", input_cmd_xkb_model },
	{ "xkb_numlock", input_cmd_xkb_numlock },
	{ "xkb_options", input_cmd_xkb_options },
	{ "xkb_rules", input_cmd_xkb_rules },
	{ "xkb_switch_layout", input_cmd_xkb_switch_layout },
	{ "xkb_variant", input_cmd_xkb_variant },
};

struct cmd_results *cmd_input(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "input", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	sway_log(SWAY_DEBUG, "entering input block: %s", argv[0]);

	config->handler_context.input_config = new_input_config(argv[0]);
	if (!config->handler_context.input_config) {
		return cmd_results_new(CMD_FAILURE, "Couldn't allocate config");
	}

	struct cmd_results *res = config_subcommand(argv + 1, argc - 1,
		input_handlers, sizeof(input_handlers));

	if ((!res || res->status == CMD_SUCCESS) &&
			strcmp(argv[1], "xkb_switch_layout") != 0) {
		// Lock settings given at run time take effect once, now; the stored
		// settings apply when a keymap is created or the config is reloaded
		int numlock = config->handler_context.input_config->xkb_numlock;
		int capslock = config->handler_context.input_config->xkb_capslock;

		char *error = NULL;
		struct input_config *ic =
			store_input_config(config->handler_context.input_config, &error);
		if (!ic) {
			free_input_config(config->handler_context.input_config);
			if (res) {
				free_cmd_results(res);
			}
			res = cmd_results_new(CMD_FAILURE, "Failed to compile keymap: %s",
					error ? error : "(details unavailable)");
			free(error);
			return res;
		}

		if (!config->reading) {
			input_manager_apply_input_config(ic);
			if (numlock != INT_MIN || capslock != INT_MIN) {
				input_manager_apply_keyboard_locks(ic->identifier,
					numlock, capslock);
			}
		}
	} else {
		free_input_config(config->handler_context.input_config);
	}

	config->handler_context.input_config = NULL;

	return res;
}
