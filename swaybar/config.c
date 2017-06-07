#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <string.h>
#include "wayland-desktop-shell-client-protocol.h"
#include "log.h"
#include "swaybar/config.h"

uint32_t parse_position(const char *position) {
	if (strcmp("top", position) == 0) {
		return DESKTOP_SHELL_PANEL_POSITION_TOP;
	} else if (strcmp("bottom", position) == 0) {
		return DESKTOP_SHELL_PANEL_POSITION_BOTTOM;
	} else if (strcmp("left", position) == 0) {
		return DESKTOP_SHELL_PANEL_POSITION_LEFT;
	} else if (strcmp("right", position) == 0) {
		return DESKTOP_SHELL_PANEL_POSITION_RIGHT;
	} else {
		return DESKTOP_SHELL_PANEL_POSITION_BOTTOM;
	}
}

char *parse_font(const char *font) {
	char *new_font = NULL;
	if (strncmp("pango:", font, 6) == 0) {
		font += 6;
	}

	new_font = strdup(font);

	return new_font;
}

struct config *init_config() {
	struct config *config = calloc(1, sizeof(struct config));
	config->status_command = NULL;
	config->pango_markup = false;
	config->position = DESKTOP_SHELL_PANEL_POSITION_BOTTOM;
	config->font = strdup("monospace 10");
	config->mode = NULL;
	config->sep_symbol = NULL;
	config->strip_workspace_numbers = false;
	config->binding_mode_indicator = true;
	config->wrap_scroll = false;
	config->workspace_buttons = true;
	config->all_outputs = false;
	config->outputs = list_new(sizeof(char *), 0);

	/* height */
	config->height = 0;

	/* colors */
	config->colors.background = 0x000000FF;
	config->colors.statusline = 0xFFFFFFFF;
	config->colors.separator = 0x666666FF;

	config->colors.focused_workspace.border = 0x4C7899FF;
	config->colors.focused_workspace.background = 0x285577FF;
	config->colors.focused_workspace.text = 0xFFFFFFFF;

	config->colors.active_workspace.border = 0x333333FF;
	config->colors.active_workspace.background = 0x5F676AFF;
	config->colors.active_workspace.text = 0xFFFFFFFF;

	config->colors.inactive_workspace.border = 0x333333FF;
	config->colors.inactive_workspace.background = 0x222222FF;
	config->colors.inactive_workspace.text = 0x888888FF;

	config->colors.urgent_workspace.border = 0x2F343AFF;
	config->colors.urgent_workspace.background = 0x900000FF;
	config->colors.urgent_workspace.text = 0xFFFFFFFF;

	config->colors.binding_mode.border = 0x2F343AFF;
	config->colors.binding_mode.background = 0x900000FF;
	config->colors.binding_mode.text = 0xFFFFFFFF;

	return config;
}

void free_config(struct config *config) {
	free(config->status_command);
	free(config->font);
	free(config->mode);
	free(config->sep_symbol);
	free(config);
}
