#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <wlr/util/log.h>
#include "swaybar/config.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "stringop.h"
#include "list.h"

uint32_t parse_position(const char *position) {
	uint32_t horiz = ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
		ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	if (strcmp("top", position) == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | horiz;
	} else if (strcmp("bottom", position) == 0) {
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | horiz;
	} else {
		wlr_log(WLR_ERROR, "Invalid position: %s, defaulting to bottom", position);
		return ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM | horiz;
	}
}

struct swaybar_config *init_config(void) {
	struct swaybar_config *config = calloc(1, sizeof(struct swaybar_config));
	config->status_command = NULL;
	config->pango_markup = false;
	config->position = parse_position("bottom");
	config->font = strdup("monospace 10");
	config->mode = strdup("dock");
	config->hidden_state = strdup("hide");
	config->sep_symbol = NULL;
	config->strip_workspace_numbers = false;
	config->strip_workspace_name = false;
	config->binding_mode_indicator = true;
	config->wrap_scroll = false;
	config->workspace_buttons = true;
	config->bindings = create_list();
	wl_list_init(&config->outputs);

	/* height */
	config->height = 0;

	/* colors */
	config->colors.background = 0x000000FF;
	config->colors.focused_background = 0x000000FF;
	config->colors.statusline = 0xFFFFFFFF;
	config->colors.focused_statusline = 0xFFFFFFFF;
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

static void free_binding(struct swaybar_binding *binding) {
	if (!binding) {
		return;
	}
	free(binding->command);
	free(binding);
}

void free_config(struct swaybar_config *config) {
	free(config->status_command);
	free(config->font);
	free(config->mode);
	free(config->hidden_state);
	free(config->sep_symbol);
	free(config->modifier);
	for (int i = 0; i < config->bindings->length; i++) {
		struct swaybar_binding *binding = config->bindings->items[i];
		free_binding(binding);
	}
	list_free(config->bindings);
	struct config_output *coutput, *tmp;
	wl_list_for_each_safe(coutput, tmp, &config->outputs, link) {
		wl_list_remove(&coutput->link);
		free(coutput->name);
		free(coutput);
	}
	free(config);
}
