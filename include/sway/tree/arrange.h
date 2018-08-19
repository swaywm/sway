#ifndef _SWAY_ARRANGE_H
#define _SWAY_ARRANGE_H
#include "sway/desktop/transaction.h"

struct sway_container;

/**
 * Arrange layout for all the children of the given container.
 */
void arrange_windows(struct sway_container *container);

#endif
