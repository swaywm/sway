#include <stdlib.h>
#include <string.h>
#include "commands.h"

static struct cmd_results *parse_border_color(struct border_colors *border_colors, const char *cmd_name, int argc, char **argv) {
	struct cmd_results *error = NULL;
	if (argc != 5) {
		return cmd_results_new(CMD_INVALID, cmd_name, "Requires exactly five color values");
	}

	uint32_t colors[5];
	int i;
	for (i = 0; i < 5; i++) {
		char buffer[10];
		error = add_color(cmd_name, buffer, argv[i]);
		if (error) {
			return error;
		}
		colors[i] = strtoul(buffer+1, NULL, 16);
	}

	border_colors->border = colors[0];
	border_colors->background = colors[1];
	border_colors->text = colors[2];
	border_colors->indicator = colors[3];
	border_colors->child_border = colors[4];

	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}

struct cmd_results *cmd_client_focused(int argc, char **argv) {
	return parse_border_color(&config->border_colors.focused, "client.focused", argc, argv);
}

struct cmd_results *cmd_client_focused_inactive(int argc, char **argv) {
	return parse_border_color(&config->border_colors.focused_inactive, "client.focused_inactive", argc, argv);
}

struct cmd_results *cmd_client_unfocused(int argc, char **argv) {
	return parse_border_color(&config->border_colors.unfocused, "client.unfocused", argc, argv);
}

struct cmd_results *cmd_client_urgent(int argc, char **argv) {
	return parse_border_color(&config->border_colors.urgent, "client.urgent", argc, argv);
}

struct cmd_results *cmd_client_placeholder(int argc, char **argv) {
	return parse_border_color(&config->border_colors.placeholder, "client.placeholder", argc, argv);
}

struct cmd_results *cmd_client_background(int argc, char **argv) {
	char buffer[10];
	struct cmd_results *error = NULL;
	uint32_t background;

	if (argc != 1) {
		return cmd_results_new(CMD_INVALID, "client.background", "Requires exactly one color value");
	}

	error = add_color("client.background", buffer, argv[0]);
	if (error) {
		return error;
	}

	background = strtoul(buffer+1, NULL, 16);
	config->border_colors.background = background;
	return cmd_results_new(CMD_SUCCESS, NULL, NULL);
}
