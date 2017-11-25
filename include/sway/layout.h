#ifndef _SWAY_LAYOUT_H
#define _SWAY_LAYOUT_H

struct sway_container;

void init_layout(void);
void add_child(struct sway_container *parent, struct sway_container *child);
enum swayc_layouts default_layout(struct sway_container *output);
void sort_workspaces(struct sway_container *output);
void arrange_windows(struct sway_container *container, double width, double height);

#endif
