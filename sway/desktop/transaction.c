#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_linux_dmabuf.h>
#include "sway/debug.h"
#include "sway/desktop/transaction.h"
#include "sway/output.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "list.h"
#include "log.h"

/**
 * How long we should wait for views to respond to the configure before giving
 * up and applying the transaction anyway.
 */
#define TIMEOUT_MS 200

struct sway_transaction_instruction {
	struct sway_transaction *transaction;
	struct sway_container *container;
	struct sway_container_state state;
	uint32_t serial;
};

struct sway_transaction *transaction_create() {
	struct sway_transaction *transaction =
		calloc(1, sizeof(struct sway_transaction));
	transaction->instructions = create_list();
	transaction->damage = create_list();
	return transaction;
}

static void transaction_destroy(struct sway_transaction *transaction) {
	int i;
	// Free instructions
	for (i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		if (instruction->container->type == C_VIEW) {
			struct sway_view *view = instruction->container->sway_view;
			for (int j = 0; j < view->instructions->length; ++j) {
				if (view->instructions->items[j] == instruction) {
					list_del(view->instructions, j);
					break;
				}
			}
		}
		free(instruction);
	}
	list_free(transaction->instructions);

	// Free damage
	for (i = 0; i < transaction->damage->length; ++i) {
		struct wlr_box *box = transaction->damage->items[i];
		free(box);
	}
	list_free(transaction->damage);

	free(transaction);
}

void transaction_add_container(struct sway_transaction *transaction,
		struct sway_container *container) {
	struct sway_transaction_instruction *instruction =
		calloc(1, sizeof(struct sway_transaction_instruction));
	instruction->transaction = transaction;
	instruction->container = container;
	memcpy(&instruction->state, &container->pending,
			sizeof(struct sway_container_state));
	list_add(transaction->instructions, instruction);
}

void transaction_add_damage(struct sway_transaction *transaction,
		struct wlr_box *_box) {
	struct wlr_box *box = calloc(1, sizeof(struct wlr_box));
	memcpy(box, _box, sizeof(struct wlr_box));
	list_add(transaction->damage, box);
}

static void save_view_texture(struct sway_view *view) {
	wlr_texture_destroy(view->saved_texture);
	view->saved_texture = NULL;

	// TODO: Copy the texture and store it in view->saved_texture.
}

static void remove_saved_view_texture(struct sway_view *view) {
	wlr_texture_destroy(view->saved_texture);
	view->saved_texture = NULL;
}

/**
 * Apply a transaction to the "current" state of the tree.
 *
 * This is mostly copying stuff from the pending state into the main swayc
 * properties, but also includes reparenting and deleting containers.
 */
static void transaction_apply(struct sway_transaction *transaction) {
	int i;
	for (i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_container_state *state = &instruction->state;
		struct sway_container *container = instruction->container;

		container->layout = state->layout;
		container->x = state->swayc_x;
		container->y = state->swayc_y;
		container->width = state->swayc_width;
		container->height = state->swayc_height;

		if (container->type == C_VIEW) {
			struct sway_view *view = container->sway_view;
			view->x = state->view_x;
			view->y = state->view_y;
			view->width = state->view_width;
			view->height = state->view_height;
			view->is_fullscreen = state->is_fullscreen;
			view->border = state->border;
			view->border_thickness = state->border_thickness;
			view->border_top = state->border_top;
			view->border_left = state->border_left;
			view->border_right = state->border_right;
			view->border_bottom = state->border_bottom;

			remove_saved_view_texture(view);
		}
	}

	// Damage
	for (i = 0; i < transaction->damage->length; ++i) {
		struct wlr_box *box = transaction->damage->items[i];
		for (int j = 0; j < root_container.children->length; ++j) {
			struct sway_container *output = root_container.children->items[j];
			output_damage_box(output->sway_output, box);
		}
	}

	update_debug_tree();
}

static int handle_timeout(void *data) {
	struct sway_transaction *transaction = data;
	wlr_log(L_DEBUG, "Transaction %p timed out (%li waiting), applying anyway",
			transaction, transaction->num_waiting);
	transaction_apply(transaction);
	transaction_destroy(transaction);
	return 0;
}

void transaction_commit(struct sway_transaction *transaction) {
	wlr_log(L_DEBUG, "Transaction %p committing with %i instructions",
			transaction, transaction->instructions->length);
	transaction->num_waiting = 0;
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		if (instruction->container->type == C_VIEW) {
			struct sway_view *view = instruction->container->sway_view;
			instruction->serial = view_configure(view,
					instruction->state.view_x,
					instruction->state.view_y,
					instruction->state.view_width,
					instruction->state.view_height);
			if (instruction->serial) {
				save_view_texture(view);
				list_add(view->instructions, instruction);
				++transaction->num_waiting;
			}
		}
	}
	if (!transaction->num_waiting) {
		// This can happen if the transaction only contains xwayland views
		wlr_log(L_DEBUG, "Transaction %p has nothing to wait for, applying",
				transaction);
		transaction_apply(transaction);
		transaction_destroy(transaction);
		return;
	}

	// Set up a timer which the views must respond within
	transaction->timer = wl_event_loop_add_timer(server.wl_event_loop,
			handle_timeout, transaction);
	wl_event_source_timer_update(transaction->timer, TIMEOUT_MS);
}

void transaction_notify_view_ready(struct sway_view *view, uint32_t serial) {
	// Find the instruction
	struct sway_transaction_instruction *instruction = NULL;
	for (int i = 0; i < view->instructions->length; ++i) {
		struct sway_transaction_instruction *tmp_instruction =
			view->instructions->items[i];
		if (tmp_instruction->serial == serial) {
			instruction = tmp_instruction;
			list_del(view->instructions, i);
			break;
		}
	}
	if (!instruction) {
		// This can happen if the view acknowledges the configure after the
		// transaction has timed out and applied.
		return;
	}
	// If all views are ready, apply the transaction
	struct sway_transaction *transaction = instruction->transaction;
	if (--transaction->num_waiting == 0) {
		wlr_log(L_DEBUG, "Transaction %p is ready, applying", transaction);
		wl_event_source_timer_update(transaction->timer, 0);
		transaction_apply(transaction);
		transaction_destroy(transaction);
	}
}
