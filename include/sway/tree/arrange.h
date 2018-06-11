#ifndef _SWAY_ARRANGE_H
#define _SWAY_ARRANGE_H
#include "sway/desktop/transaction.h"

struct sway_container;

// Remove gaps around container
void remove_gaps(struct sway_container *c);

// Add gaps around container
void add_gaps(struct sway_container *c);

/**
 * Arrange layout for all the children of the given container, and add them to
 * the given transaction.
 *
 * Use this function if you need to arrange multiple sections of the tree in one
 * transaction.
 */
void arrange_windows(struct sway_container *container,
		struct sway_transaction *transaction);

/**
 * Arrange layout for the given container and commit the transaction.
 *
 * This function is a wrapper around arrange_windows, and handles creating and
 * committing the transaction for you. Use this function if you're only doing
 * one arrange operation.
 */
void arrange_and_commit(struct sway_container *container);

#endif
