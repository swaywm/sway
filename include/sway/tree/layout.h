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
};

struct sway_container;

struct sway_root {
	struct wlr_output_layout *output_layout;

	struct wl_listener output_layout_change;

	struct wl_list xwayland_unmanaged; // sway_xwayland_unmanaged::link

	struct {
		struct wl_signal new_container;
	} events;
};

void layout_init(void);

// TODO move to tree.h
void container_add_child(struct sway_container *parent,
		struct sway_container *child);

// TODO move to tree.h
struct sway_container *container_add_sibling(struct sway_container *parent,
		struct sway_container *child);

// TODO move to tree.h
struct sway_container *container_remove_child(struct sway_container *child);

// TODO PRIVATE in tree.h
struct sway_container *container_replace_child(struct sway_container *child,
		struct sway_container *new_child);

// TODO move to layout.c
struct sway_container *container_set_layout(struct sway_container *container,
		enum sway_container_layout layout);

// TODO move to tree.h
void container_move_to(struct sway_container* container,
		struct sway_container* destination);

void container_move(struct sway_container *container,
		enum movement_direction dir, int move_amt);

enum sway_container_layout container_get_default_layout(
		struct sway_container *con);

// TODO move to output.c
void container_sort_workspaces(struct sway_container *output);

void arrange_windows(struct sway_container *container,
		double width, double height);

// TODO move to container.h
struct sway_container *container_get_in_direction(struct sway_container
		*container, struct sway_seat *seat, enum movement_direction dir);

// TODO move to tree.h
struct sway_container *container_split(struct sway_container *child,
		enum sway_container_layout layout);

#endif
