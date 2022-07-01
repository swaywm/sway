#ifndef _SWAYBAR_CONFIG_H
#define _SWAYBAR_CONFIG_H
#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>
#include "../include/config.h"
#include "list.h"
#include "util.h"
#include <pango/pangocairo.h>

struct box_colors {
	uint32_t border;
	uint32_t background;
	uint32_t text;
};

struct config_output {
	struct wl_list link; // swaybar_config::outputs
	char *name;
};

struct swaybar_binding {
	uint32_t button;
	char *command;
	bool release;
};

struct swaybar_config {
	char *status_command;
	bool pango_markup;
	uint32_t position; // zwlr_layer_surface_v1_anchor
	PangoFontDescription *font_description;
	char *sep_symbol;
	char *mode;
	char *hidden_state;
	char *modifier;
	bool strip_workspace_numbers;
	bool strip_workspace_name;
	bool binding_mode_indicator;
	bool wrap_scroll;
	bool workspace_buttons;
	uint32_t workspace_min_width;
	list_t *bindings;
	struct wl_list outputs; // config_output::link
	int height;
	int status_padding;
	int status_edge_padding;
	struct {
		int top;
		int right;
		int bottom;
		int left;
	} gaps;

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

#if HAVE_TRAY
	char *icon_theme;
	struct wl_list tray_bindings; // struct tray_binding::link
	bool tray_hidden;
	list_t *tray_outputs; // char *
	int tray_padding;
#endif
};

#if HAVE_TRAY
struct tray_binding {
	uint32_t button;
	char *command;
	struct wl_list link; // struct tray_binding::link
};

void free_tray_binding(struct tray_binding *binding);
#endif

struct swaybar_config *init_config(void);
void free_config(struct swaybar_config *config);
uint32_t parse_position(const char *position);
void free_binding(struct swaybar_binding *binding);

#endif
