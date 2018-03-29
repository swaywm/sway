#define _XOPEN_SOURCE 500
#include <stdlib.h>
#include <string.h>
#include "swaybar/config.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

uint32_t parse_position(const char *position) {
	uint32_t horiz = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
		ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	uint32_t vert = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
		ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
	if (strcmp("top", position) == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | horiz;
	} else if (strcmp("bottom", position) == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | horiz;
	} else if (strcmp("left", position) == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | vert;
	} else if (strcmp("right", position) == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT | vert;
	} else {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | horiz;
	}
}

struct swaybar_config *init_config() {
	struct swaybar_config *config = calloc(1, sizeof(struct swaybar_config));
	config->status_command = NULL;
	config->pango_markup = false;
	config->position = parse_position("bottom");
	config->font = strdup("monospace 10");
	config->mode = NULL;
	config->sep_symbol = NULL;
	config->strip_workspace_numbers = false;
	config->binding_mode_indicator = true;
	config->wrap_scroll = false;
	config->workspace_buttons = true;
	wl_list_init(&config->outputs);

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

void free_config(struct swaybar_config *config) {
	free(config->status_command);
	free(config->font);
	free(config->mode);
	free(config->sep_symbol);
	free(config);
}
