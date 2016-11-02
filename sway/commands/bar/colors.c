#include <string.h>
#include "sway/commands.h"

struct cmd_results *bar_cmd_colors(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "colors", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if (strcmp("{", argv[0]) != 0) {
		return cmd_results_new(CMD_INVALID, "colors",
				"Expected '{' at the start of colors config definition.");
	}

	return cmd_results_new(CMD_BLOCK_BAR_COLORS, NULL, NULL);
}

struct cmd_results *bar_colors_cmd_active_workspace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "active_workspace", EXPECTED_EQUAL_TO, 3))) {
		return error;
	}

	if ((error = add_color("active_workspace_border", config->current_bar->colors.active_workspace_border, argv[0]))) {
		return error;
	}

	if ((error = add_color("active_workspace_bg", config->current_bar->colors.active_workspace_bg, argv[1]))) {
		return error;
	}

	if ((error = add_color("active_workspace_text", config->current_bar->colors.active_workspace_text, argv[2]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *bar_colors_cmd_background(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "background", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if ((error = add_color("background", config->current_bar->colors.background, argv[0]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *bar_colors_cmd_focused_background(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "focused_background", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if ((error = add_color("focused_background", config->current_bar->colors.focused_background, argv[0]))) {
		return error;
	}else {
		config->current_bar->colors.has_focused_background = true;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *bar_colors_cmd_binding_mode(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "binding_mode", EXPECTED_EQUAL_TO, 3))) {
		return error;
	}

	if ((error = add_color("binding_mode_border", config->current_bar->colors.binding_mode_border, argv[0]))) {
		return error;
	} else {
		config->current_bar->colors.has_binding_mode_border = true;
	}

	if ((error = add_color("binding_mode_bg", config->current_bar->colors.binding_mode_bg, argv[1]))) {
		return error;
	} else {
		config->current_bar->colors.has_binding_mode_bg = true;
	}

	if ((error = add_color("binding_mode_text", config->current_bar->colors.binding_mode_text, argv[2]))) {
		return error;
	} else {
		config->current_bar->colors.has_binding_mode_text = true;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *bar_colors_cmd_focused_workspace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "focused_workspace", EXPECTED_EQUAL_TO, 3))) {
		return error;
	}

	if ((error = add_color("focused_workspace_border", config->current_bar->colors.focused_workspace_border, argv[0]))) {
		return error;
	}

	if ((error = add_color("focused_workspace_bg", config->current_bar->colors.focused_workspace_bg, argv[1]))) {
		return error;
	}

	if ((error = add_color("focused_workspace_text", config->current_bar->colors.focused_workspace_text, argv[2]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *bar_colors_cmd_inactive_workspace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "inactive_workspace", EXPECTED_EQUAL_TO, 3))) {
		return error;
	}

	if ((error = add_color("inactive_workspace_border", config->current_bar->colors.inactive_workspace_border, argv[0]))) {
		return error;
	}

	if ((error = add_color("inactive_workspace_bg", config->current_bar->colors.inactive_workspace_bg, argv[1]))) {
		return error;
	}

	if ((error = add_color("inactive_workspace_text", config->current_bar->colors.inactive_workspace_text, argv[2]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *bar_colors_cmd_separator(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "separator", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if ((error = add_color("separator", config->current_bar->colors.separator, argv[0]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *bar_colors_cmd_focused_separator(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "focused_separator", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if ((error = add_color("focused_separator", config->current_bar->colors.focused_separator, argv[0]))) {
		return error;
	} else {
		config->current_bar->colors.has_focused_separator = true;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *bar_colors_cmd_statusline(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "statusline", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if ((error = add_color("statusline", config->current_bar->colors.statusline, argv[0]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *bar_colors_cmd_focused_statusline(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "focused_statusline", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}

	if ((error = add_color("focused_statusline", config->current_bar->colors.focused_statusline, argv[0]))) {
		return error;
	} else {
		config->current_bar->colors.has_focused_statusline = true;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *bar_colors_cmd_urgent_workspace(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "urgent_workspace", EXPECTED_EQUAL_TO, 3))) {
		return error;
	}

	if ((error = add_color("urgent_workspace_border", config->current_bar->colors.urgent_workspace_border, argv[0]))) {
		return error;
	}

	if ((error = add_color("urgent_workspace_bg", config->current_bar->colors.urgent_workspace_bg, argv[1]))) {
		return error;
	}

	if ((error = add_color("urgent_workspace_text", config->current_bar->colors.urgent_workspace_text, argv[2]))) {
		return error;
	}

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
