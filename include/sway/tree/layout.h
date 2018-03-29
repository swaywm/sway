#ifndef _SWAY_LAYOUT_H
#define _SWAY_LAYOUT_H

#include <wlr/types/wlr_output_layout.h>
#include "sway/tree/container.h"

enum movement_direction {
	MOVE_LEFT,
	MOVE_RIGHT,
	MOVE_UP,
	MOVE_DOWN,
	MOVE_PARENT,
	MOVE_CHILD,
	MOVE_NEXT,
	MOVE_PREV,
	MOVE_FIRST
};

struct sway_container;

struct sway_root {
	struct wlr_output_layout *output_layout;

	struct wl_listener output_layout_change;

	struct wl_list unmanaged_views; // sway_view::unmanaged_view_link

	struct {
		struct wl_signal new_container;
	} events;
};

void init_layout(void);

void add_child(struct sway_container *parent, struct sway_container *child);

struct sway_container *add_sibling(struct sway_container *parent,
		struct sway_container *child);

struct sway_container *remove_child(struct sway_container *child);

enum swayc_layouts default_layout(struct sway_container *output);

void sort_workspaces(struct sway_container *output);

void arrange_windows(struct sway_container *container,
		double width, double height);

struct sway_container *get_swayc_in_direction(struct sway_container
		*container, struct sway_seat *seat, enum movement_direction dir);

#endif
