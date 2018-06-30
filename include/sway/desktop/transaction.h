#ifndef _SWAY_TRANSACTION_H
#define _SWAY_TRANSACTION_H
#include <wlr/render/wlr_texture.h>
#include "sway/tree/container.h"

/**
 * Transactions enable us to perform atomic layout updates.
 *
 * When we want to make adjustments to the layout, we create a transaction.
 * A transaction contains a list of affected containers and their new state.
 * A state might contain a new size, or new border settings, or new parent/child
 * relationships.
 *
 * Calling transaction_commit() makes sway notify of all the affected clients
 * with their new sizes. We then wait for all the views to respond with their
 * new surface sizes. When all are ready, or when a timeout has passed, we apply
 * the updates all at the same time.
 */

struct sway_transaction;

/**
 * Create a new transaction.
 */
struct sway_transaction *transaction_create(void);

/**
 * Add a container's pending state to the transaction.
 */
void transaction_add_container(struct sway_transaction *transaction,
		struct sway_container *container);

/**
 * Submit a transaction to the client views for configuration.
 */
void transaction_commit(struct sway_transaction *transaction);

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

/**
 * Get the saved texture that should be rendered for a view.
 *
 * The addresses pointed at by the width and height pointers will be populated
 * with the surface's dimensions, which may be different to the texture's
 * dimensions if output scaling is used.
 *
 * This function should only be called if it is known that the view has
 * instructions.
 */
struct wlr_texture *transaction_get_saved_texture(struct sway_view *view,
		int *width, int *height);

#endif
