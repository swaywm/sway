#ifndef _SWAY_LAYOUT_H
#define _SWAY_LAYOUT_H
#include <wlr/types/wlr_output_layout.h>
#include <wlr/render/wlr_texture.h>
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "config.h"

enum movement_direction {
	MOVE_LEFT,
	MOVE_RIGHT,
	MOVE_UP,
	MOVE_DOWN,
	MOVE_PARENT,
	MOVE_CHILD,
};

enum wlr_edges;

struct sway_container;

void container_add_child(struct sway_container *parent,
		struct sway_container *child);

struct sway_container *container_add_sibling(struct sway_container *parent,
		struct sway_container *child);

struct sway_container *container_remove_child(struct sway_container *child);

struct sway_container *container_replace_child(struct sway_container *child,
		struct sway_container *new_child);

void container_move_to(struct sway_container* container,
		struct sway_container* destination);

void container_move(struct sway_container *container,
		enum movement_direction dir, int move_amt);

enum sway_container_layout container_get_default_layout(
		struct sway_container *con);

struct sway_container *container_get_in_direction(struct sway_container
		*container, struct sway_seat *seat, enum movement_direction dir);

struct sway_container *container_split(struct sway_container *child,
		enum sway_container_layout layout);

void container_recursive_resize(struct sway_container *container,
		double amount, enum wlr_edges edge);

void container_swap(struct sway_container *con1, struct sway_container *con2);

#endif
