#define _POSIX_C_SOURCE 200809L
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "log.h"

static void switch_layout(struct wlr_keyboard *kbd, xkb_layout_index_t idx) {
	xkb_layout_index_t num_layouts = xkb_keymap_num_layouts(kbd->keymap);
	if (idx >= num_layouts) {
		return;
	}
	wlr_keyboard_notify_modifiers(kbd, kbd->modifiers.depressed,
		kbd->modifiers.latched, kbd->modifiers.locked, idx);
}

struct cmd_results *input_cmd_xkb_switch_layout(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "xkb_switch_layout", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	if (config->reading || !config->active) {
		return cmd_results_new(CMD_DEFER, NULL);
	}

	const char *layout_str = argv[0];

	char *end;
	int layout = strtol(layout_str, &end, 10);
	if (layout_str[0] == '\0' || end[0] != '\0' || layout < 0) {
		return cmd_results_new(CMD_FAILURE, "Invalid layout index.");
	}

	struct sway_input_device *dev;
	wl_list_for_each(dev, &server.input->devices, link) {
		if (strcmp(ic->identifier, "*") != 0 &&
				strcmp(ic->identifier, "type:keyboard") != 0 &&
				strcmp(ic->identifier, dev->identifier) != 0) {
			continue;
		}
		if (dev->wlr_device->type != WLR_INPUT_DEVICE_KEYBOARD) {
			continue;
		}
		switch_layout(dev->wlr_device->keyboard, layout);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
