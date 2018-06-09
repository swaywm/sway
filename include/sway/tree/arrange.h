#ifndef _SWAY_ARRANGE_H
#define _SWAY_ARRANGE_H

struct sway_container;

// Remove gaps around container
void remove_gaps(struct sway_container *c);

// Add gaps around container
void add_gaps(struct sway_container *c);

// Determine the root container's geometry, then iterate to everything below
void arrange_root(void);

// Determine the output's geometry, then iterate to everything below
void arrange_output(struct sway_container *output);

// Determine the workspace's geometry, then iterate to everything below
void arrange_workspace(struct sway_container *workspace);

// Arrange layout for all the children of the given workspace/container
void arrange_children_of(struct sway_container *parent);

#endif
