#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_linux_dmabuf.h>
#include "sway/debug.h"
#include "sway/desktop/transaction.h"
#include "sway/output.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "list.h"
#include "log.h"

/**
 * How long we should wait for views to respond to the configure before giving
 * up and applying the transaction anyway.
 */
#define TIMEOUT_MS 200

/**
 * If enabled, sway will always wait for the transaction timeout before
 * applying it, rather than applying it when the views are ready. This allows us
 * to observe the rendered state while a transaction is in progress.
 */
#define TRANSACTION_DEBUG false

struct sway_transaction {
	struct wl_event_source *timer;
	list_t *instructions;   // struct sway_transaction_instruction *
	list_t *damage;         // struct wlr_box *
	size_t num_waiting;
	size_t num_configures;
	struct sway_transaction *next;
	struct timespec create_time;
	struct timespec commit_time;
};

struct sway_transaction_instruction {
	struct sway_transaction *transaction;
	struct sway_container *container;
	struct sway_container_state state;
	struct wlr_buffer *saved_buffer;
	uint32_t serial;
	bool ready;
};

struct sway_transaction *transaction_create() {
	struct sway_transaction *transaction =
		calloc(1, sizeof(struct sway_transaction));
	transaction->instructions = create_list();
	transaction->damage = create_list();
	if (server.debug_txn_timings) {
		clock_gettime(CLOCK_MONOTONIC, &transaction->create_time);
	}
	return transaction;
}

static void remove_saved_view_buffer(
		struct sway_transaction_instruction *instruction) {
	if (instruction->saved_buffer) {
		wlr_buffer_unref(instruction->saved_buffer);
		instruction->saved_buffer = NULL;
	}
}

static void save_view_buffer(struct sway_view *view,
		struct sway_transaction_instruction *instruction) {
	if (!sway_assert(instruction->saved_buffer == NULL,
				"Didn't expect instruction to have a saved buffer already")) {
		remove_saved_view_buffer(instruction);
	}
	if (view->surface && wlr_surface_has_buffer(view->surface)) {
		wlr_buffer_ref(view->surface->buffer);
		instruction->saved_buffer = view->surface->buffer;
	}
}

static void transaction_destroy(struct sway_transaction *transaction) {
	// Free instructions
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_container *con = instruction->container;
		for (int j = 0; j < con->instructions->length; ++j) {
			if (con->instructions->items[j] == instruction) {
				list_del(con->instructions, j);
				break;
			}
		}
		if (con->destroying && !con->instructions->length) {
			container_free(con);
		}
		remove_saved_view_buffer(instruction);
		free(instruction);
	}
	list_free(transaction->instructions);

	// Free damage
	list_foreach(transaction->damage, free);
	list_free(transaction->damage);

	if (transaction->timer) {
		wl_event_source_remove(transaction->timer);
	}
	free(transaction);
}

static void copy_pending_state(struct sway_container *container,
		struct sway_container_state *state) {
	state->layout = container->layout;
	state->swayc_x = container->x;
	state->swayc_y = container->y;
	state->swayc_width = container->width;
	state->swayc_height = container->height;
	state->has_gaps = container->has_gaps;
	state->current_gaps = container->current_gaps;
	state->gaps_inner = container->gaps_inner;
	state->gaps_outer = container->gaps_outer;
	state->parent = container->parent;

	if (container->type == C_VIEW) {
		struct sway_view *view = container->sway_view;
		state->view_x = view->x;
		state->view_y = view->y;
		state->view_width = view->width;
		state->view_height = view->height;
		state->is_fullscreen = view->is_fullscreen;
		state->border = view->border;
		state->border_thickness = view->border_thickness;
		state->border_top = view->border_top;
		state->border_left = view->border_left;
		state->border_right = view->border_right;
		state->border_bottom = view->border_bottom;
	} else if (container->type == C_WORKSPACE) {
		state->ws_fullscreen = container->sway_workspace->fullscreen;
		state->ws_floating = container->sway_workspace->floating;
		state->children = create_list();
		list_cat(state->children, container->children);
	} else {
		state->children = create_list();
		list_cat(state->children, container->children);
	}
}

static bool transaction_has_container(struct sway_transaction *transaction,
		struct sway_container *container) {
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		if (instruction->container == container) {
			return true;
		}
	}
	return false;
}

void transaction_add_container(struct sway_transaction *transaction,
		struct sway_container *container) {
	if (transaction_has_container(transaction, container)) {
		return;
	}
	struct sway_transaction_instruction *instruction =
		calloc(1, sizeof(struct sway_transaction_instruction));
	instruction->transaction = transaction;
	instruction->container = container;

	copy_pending_state(container, &instruction->state);

	if (container->type == C_VIEW) {
		save_view_buffer(container->sway_view, instruction);
	}
	list_add(transaction->instructions, instruction);
}

void transaction_add_damage(struct sway_transaction *transaction,
		struct wlr_box *_box) {
	struct wlr_box *box = calloc(1, sizeof(struct wlr_box));
	memcpy(box, _box, sizeof(struct wlr_box));
	list_add(transaction->damage, box);
}

/**
 * Apply a transaction to the "current" state of the tree.
 */
static void transaction_apply(struct sway_transaction *transaction) {
	wlr_log(L_DEBUG, "Applying transaction %p", transaction);
	if (server.debug_txn_timings) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		struct timespec *create = &transaction->create_time;
		struct timespec *commit = &transaction->commit_time;
		float ms_arranging = (commit->tv_sec - create->tv_sec) * 1000 +
			(commit->tv_nsec - create->tv_nsec) / 1000000.0;
		float ms_waiting = (now.tv_sec - commit->tv_sec) * 1000 +
			(now.tv_nsec - commit->tv_nsec) / 1000000.0;
		float ms_total = ms_arranging + ms_waiting;
		wlr_log(L_DEBUG, "Transaction %p: %.1fms arranging, %.1fms waiting, "
				"%.1fms total (%.1f frames if 60hz)", transaction,
				ms_arranging, ms_waiting, ms_total, ms_total / (1000 / 60));
	}
	int i;
	// Apply the instruction state to the container's current state
	for (i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_container *container = instruction->container;

		// There are separate children lists for each instruction state, the
		// container's current state and the container's pending state
		// (ie. con->children). The list itself needs to be freed here.
		// Any child containers which are being deleted will be cleaned up in
		// transaction_destroy().
		list_free(container->current.children);

		memcpy(&container->current, &instruction->state,
				sizeof(struct sway_container_state));
	}

	// Apply damage
	for (i = 0; i < transaction->damage->length; ++i) {
		struct wlr_box *box = transaction->damage->items[i];
		for (int j = 0; j < root_container.children->length; ++j) {
			struct sway_container *output = root_container.children->items[j];
			output_damage_box(output->sway_output, box);
		}
	}
}

static void transaction_progress_queue() {
	struct sway_transaction *transaction = server.head_transaction;
	struct sway_transaction *next = NULL;
	while (transaction && !transaction->num_waiting) {
		next = transaction->next;
		transaction_apply(transaction);
		transaction_destroy(transaction);
		transaction = next;
	}
	server.head_transaction = transaction;
}

static int handle_timeout(void *data) {
	struct sway_transaction *transaction = data;
	wlr_log(L_DEBUG, "Transaction %p timed out (%li waiting)",
			transaction, transaction->num_waiting);
	transaction->num_waiting = 0;
	transaction_progress_queue();
	return 0;
}

static bool should_configure(struct sway_container *con,
		struct sway_transaction_instruction *instruction) {
	if (con->type != C_VIEW) {
		return false;
	}
	if (con->destroying) {
		return false;
	}
	// The settled dimensions are what size the view will be once any pending
	// configures have applied (excluding the one we might be configuring now).
	// If these match the dimensions that this transaction wants then we don't
	// need to configure it.
	int settled_width = con->current.view_width;
	int settled_height = con->current.view_height;
	if (con->instructions->length) {
		struct sway_transaction_instruction *last_instruction =
			con->instructions->items[con->instructions->length - 1];
		settled_width = last_instruction->state.view_width;
		settled_height = last_instruction->state.view_height;
	}
	if (settled_width == instruction->state.view_width &&
			settled_height == instruction->state.view_height) {
		return false;
	}
	return true;
}

void transaction_commit(struct sway_transaction *transaction) {
	wlr_log(L_DEBUG, "Transaction %p committing with %i instructions",
			transaction, transaction->instructions->length);
	transaction->num_waiting = 0;
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_container *con = instruction->container;
		if (should_configure(con, instruction)) {
			instruction->serial = view_configure(con->sway_view,
					instruction->state.view_x,
					instruction->state.view_y,
					instruction->state.view_width,
					instruction->state.view_height);
			++transaction->num_waiting;
		}
		list_add(con->instructions, instruction);
	}
	transaction->num_configures = transaction->num_waiting;
	if (server.debug_txn_timings) {
		clock_gettime(CLOCK_MONOTONIC, &transaction->commit_time);
	}
	if (server.head_transaction) {
		// There is another transaction in progress - we must add this one to
		// the queue so we complete after it.
		struct sway_transaction *tail = server.head_transaction;
		while (tail->next) {
			tail = tail->next;
		}
		tail->next = transaction;
	} else if (transaction->num_waiting) {
		// There are no other transactions, but we're not applying immediately
		// so we must jump in the queue so others will queue behind us.
		server.head_transaction = transaction;
	} else {
		// There are no other transactions in progress, and this one has nothing
		// to wait for, so we can skip the queue.
		wlr_log(L_DEBUG, "Transaction %p has nothing to wait for", transaction);
		transaction_apply(transaction);
		transaction_destroy(transaction);
		return;
	}

	if (transaction->num_waiting) {
		// Set up a timer which the views must respond within
		transaction->timer = wl_event_loop_add_timer(server.wl_event_loop,
				handle_timeout, transaction);
		wl_event_source_timer_update(transaction->timer, TIMEOUT_MS);
	}

	// The debug tree shows the pending/live tree. Here is a good place to
	// update it, because we make a transaction every time we change the pending
	// tree.
	update_debug_tree();
}

static void set_instruction_ready(
		struct sway_transaction_instruction *instruction) {
	instruction->ready = true;
	struct sway_transaction *transaction = instruction->transaction;

	if (server.debug_txn_timings) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		struct timespec *start = &transaction->commit_time;
		float ms = (now.tv_sec - start->tv_sec) * 1000 +
			(now.tv_nsec - start->tv_nsec) / 1000000.0;
		wlr_log(L_DEBUG, "Transaction %p: %li/%li ready in %.1fms (%s)",
				transaction,
				transaction->num_configures - transaction->num_waiting + 1,
				transaction->num_configures, ms,
				instruction->container->name);

	}

	// If all views are ready, apply the transaction.
	// If the transaction has timed out then its num_waiting will be 0 already.
	if (transaction->num_waiting > 0 && --transaction->num_waiting == 0) {
#if !TRANSACTION_DEBUG
		wlr_log(L_DEBUG, "Transaction %p is ready", transaction);
		wl_event_source_timer_update(transaction->timer, 0);
		transaction_progress_queue();
#endif
	}
}

void transaction_notify_view_ready(struct sway_view *view, uint32_t serial) {
	for (int i = 0; i < view->swayc->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			view->swayc->instructions->items[i];
		if (instruction->serial == serial && !instruction->ready) {
			set_instruction_ready(instruction);
			return;
		}
	}
}

void transaction_notify_view_ready_by_size(struct sway_view *view,
		int width, int height) {
	for (int i = 0; i < view->swayc->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			view->swayc->instructions->items[i];
		if (!instruction->ready && instruction->state.view_width == width &&
				instruction->state.view_height == height) {
			set_instruction_ready(instruction);
			return;
		}
	}
}

struct wlr_texture *transaction_get_texture(struct sway_view *view) {
	if (!view->swayc || !view->swayc->instructions->length) {
		return view->surface->buffer->texture;
	}
	struct sway_transaction_instruction *instruction =
		view->swayc->instructions->items[0];
	return instruction->saved_buffer ?
		instruction->saved_buffer->texture : NULL;
}
