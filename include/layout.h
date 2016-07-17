#ifndef _SWAY_LAYOUT_H
#define _SWAY_LAYOUT_H

#include <wlc/wlc.h>
#include "log.h"
#include "list.h"
#include "container.h"
#include "focus.h"

extern list_t *scratchpad;

extern int min_sane_w;
extern int min_sane_h;

// Set initial values for root_container
void init_layout(void);

// Returns the index of child for its parent
int index_child(const swayc_t *child);

// Adds child to parent, if parent has no focus, it is set to child
// parent must be of type C_WORKSPACE or C_CONTAINER
void add_child(swayc_t *parent, swayc_t *child);

// Adds child to parent at index, if parent has no focus, it is set to child
// parent must be of type C_WORKSPACE or C_CONTAINER
void insert_child(swayc_t *parent, swayc_t *child, int index);

// Adds child as floating window to ws, if there is no focus it is set to child.
// ws must be of type C_WORKSPACE
void add_floating(swayc_t *ws, swayc_t *child);

// insert child after sibling in parents children.
swayc_t *add_sibling(swayc_t *sibling, swayc_t *child);

// Replace child with new_child in parents children
// new_child will inherit childs geometry, childs geometry will be reset
// if parents focus is on child, it will be changed to new_child
swayc_t *replace_child(swayc_t *child, swayc_t *new_child);

// Remove child from its parent, if focus is on child, focus will be changed to
// a sibling, or to a floating window, or NULL
swayc_t *remove_child(swayc_t *child);

// 2 containers are swapped, they inherit eachothers focus
void swap_container(swayc_t *a, swayc_t *b);

// 2 Containers geometry are swapped, used with `swap_container`
void swap_geometry(swayc_t *a, swayc_t *b);

void move_floating_container(swayc_t *container, int dx, int dy);
void move_container(swayc_t* container, enum movement_direction direction);
void move_container_to(swayc_t* container, swayc_t* destination);
void move_workspace_to(swayc_t* workspace, swayc_t* destination);

// Layout
/**
 * Update child container geometries when switching between layouts.
 */
void update_layout_geometry(swayc_t *parent, enum swayc_layouts prev_layout);
void update_geometry(swayc_t *view);
void arrange_windows(swayc_t *container, double width, double height);

swayc_t *get_focused_container(swayc_t *parent);
swayc_t *get_swayc_in_direction(swayc_t *container, enum movement_direction dir);
swayc_t *get_swayc_in_direction_under(swayc_t *container, enum movement_direction dir, swayc_t *limit);

void recursive_resize(swayc_t *container, double amount, enum wlc_resize_edge edge);

void layout_log(const swayc_t *c, int depth);
void swayc_log(log_importance_t verbosity, swayc_t *cont, const char* format, ...) __attribute__((format(printf,3,4)));

/**
 * Get default layout.
 */
enum swayc_layouts default_layout(swayc_t *output);

#endif
