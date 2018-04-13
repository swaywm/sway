#include <string.h>
#include "sway/commands.h"

static struct cmd_results *parse_single_color(char **color,
		const char *cmd_name, int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, cmd_name, EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (!*color && !(*color = malloc(10))) {
		return NULL;
	}
	error = add_color(cmd_name, *color, argv[0]);
	if (error) {
		return error;
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

static struct cmd_results *parse_three_colors(char ***colors,
		const char *cmd_name, int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (argc != 3) {
		return cmd_results_new(CMD_INVALID,
				cmd_name, "Requires exactly three color values");
	}
	for (size_t i = 0; i < 3; i++) {
		if (!*colors[i] && !(*(colors[i]) = malloc(10))) {
			return NULL;
		}
		error = add_color(cmd_name, *(colors[i]), argv[i]);
		if (error) {
			return error;
		}
	}
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

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
	char **colors[3] = {
		&(config->current_bar->colors.active_workspace_border),
		&(config->current_bar->colors.active_workspace_bg),
		&(config->current_bar->colors.active_workspace_text)
	};
	return parse_three_colors(colors, "active_workspace", argc, argv);
}

struct cmd_results *bar_colors_cmd_background(int argc, char **argv) {
	return parse_single_color(&(config->current_bar->colors.background),
			"background", argc, argv);
}

struct cmd_results *bar_colors_cmd_focused_background(int argc, char **argv) {
	return parse_single_color(&(config->current_bar->colors.focused_background),
			"focused_background", argc, argv);
}

struct cmd_results *bar_colors_cmd_binding_mode(int argc, char **argv) {
	char **colors[3] = {
		&(config->current_bar->colors.binding_mode_border),
		&(config->current_bar->colors.binding_mode_bg),
		&(config->current_bar->colors.binding_mode_text)
	};
	return parse_three_colors(colors, "binding_mode", argc, argv);
}

struct cmd_results *bar_colors_cmd_focused_workspace(int argc, char **argv) {
	char **colors[3] = {
		&(config->current_bar->colors.focused_workspace_border),
		&(config->current_bar->colors.focused_workspace_bg),
		&(config->current_bar->colors.focused_workspace_text)
	};
	return parse_three_colors(colors, "focused_workspace", argc, argv);
}

struct cmd_results *bar_colors_cmd_inactive_workspace(int argc, char **argv) {
	char **colors[3] = {
		&(config->current_bar->colors.inactive_workspace_border),
		&(config->current_bar->colors.inactive_workspace_bg),
		&(config->current_bar->colors.inactive_workspace_text)
	};
	return parse_three_colors(colors, "inactive_workspace", argc, argv);
}

struct cmd_results *bar_colors_cmd_separator(int argc, char **argv) {
	return parse_single_color(&(config->current_bar->colors.separator),
			"separator", argc, argv);
}

struct cmd_results *bar_colors_cmd_focused_separator(int argc, char **argv) {
	return parse_single_color(&(config->current_bar->colors.focused_separator),
			"focused_separator", argc, argv);
}

struct cmd_results *bar_colors_cmd_statusline(int argc, char **argv) {
	return parse_single_color(&(config->current_bar->colors.statusline),
			"statusline", argc, argv);
}

struct cmd_results *bar_colors_cmd_focused_statusline(int argc, char **argv) {
	return parse_single_color(&(config->current_bar->colors.focused_separator),
			"focused_separator", argc, argv);
}

struct cmd_results *bar_colors_cmd_urgent_workspace(int argc, char **argv) {
	char **colors[3] = {
		&(config->current_bar->colors.urgent_workspace_border),
		&(config->current_bar->colors.urgent_workspace_bg),
		&(config->current_bar->colors.urgent_workspace_text)
	};
	return parse_three_colors(colors, "urgent_workspace", argc, argv);
}
