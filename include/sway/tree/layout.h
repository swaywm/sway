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

void layout_init(void);

void container_add_child(struct sway_container *parent, struct sway_container *child);

struct sway_container *container_add_sibling(struct sway_container *parent,
		struct sway_container *child);

struct sway_container *container_remove_child(struct sway_container *child);

struct sway_container *container_reap_empty(struct sway_container *container);

void container_move_to(struct sway_container* container,
		struct sway_container* destination);

enum sway_container_layout container_get_default_layout(struct sway_container *output);

void container_sort_workspaces(struct sway_container *output);

void arrange_windows(struct sway_container *container,
		double width, double height);

struct sway_container *container_get_in_direction(struct sway_container
		*container, struct sway_seat *seat, enum movement_direction dir);

#endif
