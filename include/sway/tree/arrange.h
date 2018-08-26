#ifndef _SWAY_ARRANGE_H
#define _SWAY_ARRANGE_H

struct sway_container;

void arrange_container(struct sway_container *container);

void arrange_workspace(struct sway_container *workspace);

void arrange_output(struct sway_container *output);

void arrange_root(void);

void arrange_windows(struct sway_container *container);

#endif
