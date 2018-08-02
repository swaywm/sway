#ifndef _SWAY_TRANSACTION_H
#define _SWAY_TRANSACTION_H
#include <wlr/render/wlr_texture.h>
#include "sway/tree/container.h"

/**
 * Transactions enable us to perform atomic layout updates.
 *
 * A transaction contains a list of containers and their new state.
 * A state might contain a new size, or new border settings, or new parent/child
 * relationships.
 *
 * Committing a transaction makes sway notify of all the affected clients with
 * their new sizes. We then wait for all the views to respond with their new
 * surface sizes. When all are ready, or when a timeout has passed, we apply the
 * updates all at the same time.
 *
 * When we want to make adjustments to the layout, we change the pending state
 * in containers, mark them as dirty and call transaction_commit_dirty(). This
 * create and commits a transaction from the dirty containers.
 */

/**
 * Find all dirty containers, create and commit a transaction containing them,
 * and unmark them as dirty.
 */
void transaction_commit_dirty(void);

/**
 * Notify the transaction system that a view is ready for the new layout.
 *
 * When all views in the transaction are ready, the layout will be applied.
 */
void transaction_notify_view_ready(struct sway_view *view, uint32_t serial);

/**
 * Notify the transaction system that a view is ready for the new layout, but
 * identifying the instruction by width and height rather than by serial.
 *
 * This is used by xwayland views, as they don't have serials.
 */
void transaction_notify_view_ready_by_size(struct sway_view *view,
		int width, int height);

#endif
