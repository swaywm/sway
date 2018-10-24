#define _XOPEN_SOURCE 700
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/keyboard.h"
#include "log.h"

static void switch_xkb_layout(struct xkb_state *xkb_state,
		xkb_layout_index_t layout_index) {
	xkb_mod_mask_t depressed = xkb_state_serialize_mods(xkb_state, XKB_STATE_MODS_DEPRESSED);
	xkb_mod_mask_t latched = xkb_state_serialize_mods(xkb_state, XKB_STATE_MODS_LATCHED);
	xkb_mod_mask_t locked = xkb_state_serialize_mods(xkb_state, XKB_STATE_MODS_LOCKED);
	xkb_state_update_mask(xkb_state, depressed, latched, locked, 0, 0, layout_index);
}

struct cmd_results *input_cmd_xkb_current_layout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "xkb_current_layout", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "xkb_current_layout",
				"No input device defined.");
	}

	int layout_index = atoi(argv[0]);
	if (layout_index < 0) {
		return cmd_results_new(CMD_INVALID, "xkb_current_layout",
			"Layout index cannot be negative");
	}

	struct sway_input_device *input_device = NULL;
	bool wildcard = strcmp(ic->identifier, "*") == 0;
	wl_list_for_each(input_device, &server.input->devices, link) {
		if (strcmp(input_device->identifier, ic->identifier) == 0
				|| wildcard) {
			if (input_device->wlr_device->type == WLR_INPUT_DEVICE_KEYBOARD) {
				switch_xkb_layout(input_device->wlr_device->keyboard->xkb_state,
						layout_index);
				wlr_log(WLR_DEBUG, "switch-xkb-layout '%s' -> %i",
						input_device->identifier,  layout_index);
			}
		}
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
