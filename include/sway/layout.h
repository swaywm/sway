#ifndef _SWAY_LAYOUT_H
#define _SWAY_LAYOUT_H

#include <wlr/types/wlr_output_layout.h>

struct sway_container;

struct sway_root {
	struct wlr_output_layout *output_layout;

	struct wl_listener output_layout_change;
};

void init_layout(void);
void add_child(struct sway_container *parent, struct sway_container *child);
struct sway_container *remove_child(struct sway_container *child);
enum swayc_layouts default_layout(struct sway_container *output);
void sort_workspaces(struct sway_container *output);
void arrange_windows(struct sway_container *container, double width, double height);

#endif
