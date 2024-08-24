#ifndef _SWAY_ARRANGE_H
#define _SWAY_ARRANGE_H

struct sway_output;
struct sway_workspace;
struct sway_container;
struct sway_node;

void arrange_container(struct sway_container *container);
void next_sibling_container_geometry(struct sway_container *child, struct sway_container *sibling, bool fullscreen);

void arrange_workspace(struct sway_workspace *workspace);

void arrange_output(struct sway_output *output);

void arrange_root(void);

void arrange_node(struct sway_node *node);

#endif
