#ifndef _SWAYBAR_CONFIG_H
#define _SWAYBAR_CONFIG_H
#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include "config.h"
#include "util.h"

struct box_colors {
	uint32_t border;
	uint32_t background;
	uint32_t text;
};

struct config_output {
	struct wl_list link;
	char *name;
	size_t index;
};

struct swaybar_config {
	char *status_command;
	bool pango_markup;
	uint32_t position; // zwlr_layer_surface_v1_anchor
	char *font;
	char *sep_symbol;
	char *mode;
	bool mode_pango_markup;
	bool strip_workspace_numbers;
	bool binding_mode_indicator;
	bool wrap_scroll;
	bool workspace_buttons;
	struct wl_list outputs;
	bool all_outputs;
	int height;

#ifdef ENABLE_TRAY
	// TODO TRAY
	// output, render, buttons
	uint32_t tray_padding;
#endif

	struct {
		uint32_t background;
		uint32_t statusline;
		uint32_t separator;

		uint32_t focused_background;
		uint32_t focused_statusline;
		uint32_t focused_separator;

		struct box_colors focused_workspace;
		struct box_colors active_workspace;
		struct box_colors inactive_workspace;
		struct box_colors urgent_workspace;
		struct box_colors binding_mode;
	} colors;
};

struct swaybar_config *init_config();
void free_config(struct swaybar_config *config);
uint32_t parse_position(const char *position);

#endif
