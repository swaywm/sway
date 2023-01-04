#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include "sway/config.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "log.h"

struct xkb_switch_layout_action {
	struct wlr_keyboard *keyboard;
	xkb_layout_index_t layout;
};

static void switch_layout(struct wlr_keyboard *kbd, xkb_layout_index_t idx) {
	xkb_layout_index_t num_layouts = xkb_keymap_num_layouts(kbd->keymap);
	if (idx >= num_layouts) {
		return;
	}
	wlr_keyboard_notify_modifiers(kbd, kbd->modifiers.depressed,
		kbd->modifiers.latched, kbd->modifiers.locked, idx);
}

static xkb_layout_index_t get_current_layout_index(struct wlr_keyboard *kbd) {
	xkb_layout_index_t num_layouts = xkb_keymap_num_layouts(kbd->keymap);
	assert(num_layouts > 0);

	xkb_layout_index_t layout_idx;
	for (layout_idx = 0; layout_idx < num_layouts; layout_idx++) {
		if (xkb_state_layout_index_is_active(kbd->xkb_state,
				layout_idx, XKB_STATE_LAYOUT_EFFECTIVE)) {
			break;
		}
	}
	return layout_idx;
}

static xkb_layout_index_t get_layout_relative(struct wlr_keyboard *kbd, int dir) {
	xkb_layout_index_t num_layouts = xkb_keymap_num_layouts(kbd->keymap);
	xkb_layout_index_t idx = get_current_layout_index(kbd);
	return (idx + num_layouts + dir) % num_layouts;
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
	int relative, layout;

	if (strcmp(layout_str, "next") == 0) {
		relative = 1;
	} else if (strcmp(layout_str, "prev") == 0) {
		relative = -1;
	} else {
		char *end;
		layout = strtol(layout_str, &end, 10);
		if (layout_str[0] == '\0' || end[0] != '\0') {
			return cmd_results_new(CMD_FAILURE, "Invalid argument.");
		} else if (layout < 0) {
			return cmd_results_new(CMD_FAILURE, "Invalid layout index.");
		}
		relative = 0;
	}

	struct xkb_switch_layout_action *actions = calloc(
		wl_list_length(&server.input->devices),
		sizeof(struct xkb_switch_layout_action));
	size_t actions_len = 0;

	if (!actions) {
		return cmd_results_new(CMD_FAILURE, "Unable to allocate actions");
	}

	/* Calculate new indexes first because switching a layout in one
	   keyboard may result in a change on other keyboards as well because
	   of keyboard groups. */
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

		struct xkb_switch_layout_action *action =
			&actions[actions_len++];

		action->keyboard = wlr_keyboard_from_input_device(dev->wlr_device);
		if (relative) {
			action->layout = get_layout_relative(action->keyboard, relative);
		} else {
			action->layout = layout;
		}
	}

	for (size_t i = 0; i < actions_len; i++) {
		switch_layout(actions[i].keyboard, actions[i].layout);
	}
	free(actions);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
