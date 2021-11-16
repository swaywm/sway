#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"

static void set_tool_mode(struct input_config *ic,
		enum wlr_tablet_tool_type type, enum sway_tablet_tool_mode mode) {
	for (int i = 0; i < ic->tools->length; i++) {
		struct input_config_tool *tool = ic->tools->items[i];
		if (tool->type == type) {
			tool->mode = mode;
			return;
		}
	}

	struct input_config_tool *tool = calloc(1, sizeof(*tool));
	if (tool) {
		tool->type = type;
		tool->mode = mode;
		list_add(ic->tools, tool);
	}
}

struct cmd_results *input_cmd_tool_mode(int argc, char **argv) {
	struct cmd_results *error;
	if ((error = checkarg(argc, "tool_mode", EXPECTED_AT_LEAST, 2))) {
		return error;
	}

	struct input_config *ic = config->handler_context.input_config;
	if (!ic) {
		return cmd_results_new(CMD_FAILURE, "No input device defined.");
	}

	enum sway_tablet_tool_mode tool_mode;
	if (!strcasecmp(argv[1], "absolute")) {
		tool_mode = SWAY_TABLET_TOOL_MODE_ABSOLUTE;
	} else if (!strcasecmp(argv[1], "relative")) {
		tool_mode = SWAY_TABLET_TOOL_MODE_RELATIVE;
	} else {
		goto invalid_command;
	}

	if (!strcasecmp(argv[0], "*")) {
		set_tool_mode(ic, WLR_TABLET_TOOL_TYPE_PEN, tool_mode);
		set_tool_mode(ic, WLR_TABLET_TOOL_TYPE_ERASER, tool_mode);
		set_tool_mode(ic, WLR_TABLET_TOOL_TYPE_BRUSH, tool_mode);
		set_tool_mode(ic, WLR_TABLET_TOOL_TYPE_PENCIL, tool_mode);
		set_tool_mode(ic, WLR_TABLET_TOOL_TYPE_AIRBRUSH, tool_mode);
	} else {
		enum wlr_tablet_tool_type tool_type;
		if (!strcasecmp(argv[0], "pen")) {
			tool_type = WLR_TABLET_TOOL_TYPE_PEN;
		} else if (!strcasecmp(argv[0], "eraser")) {
			tool_type = WLR_TABLET_TOOL_TYPE_ERASER;
		} else if (!strcasecmp(argv[0], "brush")) {
			tool_type = WLR_TABLET_TOOL_TYPE_BRUSH;
		} else if (!strcasecmp(argv[0], "pencil")) {
			tool_type = WLR_TABLET_TOOL_TYPE_PENCIL;
		} else if (!strcasecmp(argv[0], "airbrush")) {
			tool_type = WLR_TABLET_TOOL_TYPE_AIRBRUSH;
		} else {
			goto invalid_command;
		}

		set_tool_mode(ic, tool_type, tool_mode);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);

invalid_command:
	return cmd_results_new(CMD_INVALID,
		"Expected 'tool_mode <tool> <absolute|relative>'");
}
