#ifndef _SWAY_ARRANGE_H
#define _SWAY_ARRANGE_H
#include "sway/desktop/transaction.h"

struct sway_container;

// Remove gaps around container
void remove_gaps(struct sway_container *c);

// Add gaps around container
void add_gaps(struct sway_container *c);

/**
 * Arrange layout for all the children of the given container.
 */
void arrange_windows(struct sway_container *container);

#endif
