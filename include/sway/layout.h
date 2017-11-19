#ifndef _SWAY_LAYOUT_H
#define _SWAY_LAYOUT_H

struct sway_container;

void init_layout(void);
void add_child(struct sway_container *parent, struct sway_container *child);

#endif
