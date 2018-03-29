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

swayc_t *add_sibling(swayc_t *parent, swayc_t *child);

struct sway_container *remove_child(struct sway_container *child);

enum swayc_layouts default_layout(struct sway_container *output);

void sort_workspaces(struct sway_container *output);

void arrange_windows(struct sway_container *container,
		double width, double height);

swayc_t *get_swayc_in_direction(swayc_t *container,
		struct sway_seat *seat, enum movement_direction dir);

#endif
