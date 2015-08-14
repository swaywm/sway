#ifndef _SWAY_LAYOUT_H
#define _SWAY_LAYOUT_H

#include <wlc/wlc.h>
#include "list.h"
#include "container.h"

extern swayc_t root_container;

void init_layout(void);

void add_child(swayc_t *parent, swayc_t *child);
//Returns parent container wihch needs to be rearranged.
swayc_t *add_sibling(swayc_t *sibling, swayc_t *child);
swayc_t *replace_child(swayc_t *child, swayc_t *new_child);
swayc_t *remove_child(swayc_t *parent, swayc_t *child);

void unfocus_all(swayc_t *container);
void focus_view(swayc_t *view);
void arrange_windows(swayc_t *container, int width, int height);
swayc_t *get_focused_container(swayc_t *parent);

swayc_t *get_swayc_for_handle(wlc_handle handle, swayc_t *parent);

#endif
