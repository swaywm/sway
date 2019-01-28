#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wlr/types/wlr_buffer.h>
#include "sway/config.h"
#include "sway/debug.h"
#include "sway/desktop.h"
#include "sway/desktop/idle_inhibit_v1.h"
#include "sway/desktop/transaction.h"
#include "sway/input/cursor.h"
#include "sway/input/input-manager.h"
#include "sway/output.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "list.h"
#include "log.h"

struct sway_transaction {
	struct wl_event_source *timer;
	list_t *instructions;   // struct sway_transaction_instruction *
	size_t num_waiting;
	size_t num_configures;
	struct timespec commit_time;
};

struct sway_transaction_instruction {
	struct sway_transaction *transaction;
	struct sway_node *node;
	union {
		struct sway_output_state output_state;
		struct sway_workspace_state workspace_state;
		struct sway_container_state container_state;
	};
	uint32_t serial;
};

static struct sway_transaction *transaction_create(void) {
	struct sway_transaction *transaction =
		calloc(1, sizeof(struct sway_transaction));
	if (!sway_assert(transaction, "Unable to allocate transaction")) {
		return NULL;
	}
	transaction->instructions = create_list();
	return transaction;
}

static void transaction_destroy(struct sway_transaction *transaction) {
	// Free instructions
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_node *node = instruction->node;
		node->ntxnrefs--;
		if (node->instruction == instruction) {
			node->instruction = NULL;
		}
		if (node->destroying && node->ntxnrefs == 0) {
			switch (node->type) {
			case N_ROOT:
				sway_assert(false, "Never reached");
				break;
			case N_OUTPUT:
				output_destroy(node->sway_output);
				break;
			case N_WORKSPACE:
				workspace_destroy(node->sway_workspace);
				break;
			case N_CONTAINER:
				container_destroy(node->sway_container);
				break;
			}
		}
		free(instruction);
	}
	list_free(transaction->instructions);

	if (transaction->timer) {
		wl_event_source_remove(transaction->timer);
	}
	free(transaction);
}

static void copy_output_state(struct sway_output *output,
		struct sway_transaction_instruction *instruction) {
	struct sway_output_state *state = &instruction->output_state;
	state->workspaces = create_list();
	list_cat(state->workspaces, output->workspaces);

	state->active_workspace = output_get_active_workspace(output);
}

static void copy_workspace_state(struct sway_workspace *ws,
		struct sway_transaction_instruction *instruction) {
	struct sway_workspace_state *state = &instruction->workspace_state;

	state->fullscreen = ws->fullscreen;
	state->x = ws->x;
	state->y = ws->y;
	state->width = ws->width;
	state->height = ws->height;
	state->layout = ws->layout;

	state->output = ws->output;
	state->floating = create_list();
	state->tiling = create_list();
	list_cat(state->floating, ws->floating);
	list_cat(state->tiling, ws->tiling);

	struct sway_seat *seat = input_manager_current_seat();
	state->focused = seat_get_focus(seat) == &ws->node;

	// Set focused_inactive_child to the direct tiling child
	struct sway_container *focus = seat_get_focus_inactive_tiling(seat, ws);
	if (focus) {
		while (focus->parent) {
			focus = focus->parent;
		}
	}
	state->focused_inactive_child = focus;
}

static void copy_container_state(struct sway_container *container,
		struct sway_transaction_instruction *instruction) {
	struct sway_container_state *state = &instruction->container_state;

	state->layout = container->layout;
	state->x = container->x;
	state->y = container->y;
	state->width = container->width;
	state->height = container->height;
	state->fullscreen_mode = container->fullscreen_mode;
	state->parent = container->parent;
	state->workspace = container->workspace;
	state->border = container->border;
	state->border_thickness = container->border_thickness;
	state->border_top = container->border_top;
	state->border_left = container->border_left;
	state->border_right = container->border_right;
	state->border_bottom = container->border_bottom;
	state->content_x = container->content_x;
	state->content_y = container->content_y;
	state->content_width = container->content_width;
	state->content_height = container->content_height;

	if (!container->view) {
		state->children = create_list();
		list_cat(state->children, container->children);
	}

	struct sway_seat *seat = input_manager_current_seat();
	state->focused = seat_get_focus(seat) == &container->node;

	if (!container->view) {
		struct sway_node *focus =
			seat_get_active_tiling_child(seat, &container->node);
		state->focused_inactive_child = focus ? focus->sway_container : NULL;
	}
}

static void transaction_add_node(struct sway_transaction *transaction,
		struct sway_node *node) {
	struct sway_transaction_instruction *instruction =
		calloc(1, sizeof(struct sway_transaction_instruction));
	if (!sway_assert(instruction, "Unable to allocate instruction")) {
		return;
	}
	instruction->transaction = transaction;
	instruction->node = node;

	switch (node->type) {
	case N_ROOT:
		break;
	case N_OUTPUT:
		copy_output_state(node->sway_output, instruction);
		break;
	case N_WORKSPACE:
		copy_workspace_state(node->sway_workspace, instruction);
		break;
	case N_CONTAINER:
		copy_container_state(node->sway_container, instruction);
		break;
	}

	list_add(transaction->instructions, instruction);
	node->ntxnrefs++;
}

static void apply_output_state(struct sway_output *output,
		struct sway_output_state *state) {
	output_damage_whole(output);
	list_free(output->current.workspaces);
	memcpy(&output->current, state, sizeof(struct sway_output_state));
	output_damage_whole(output);
}

static void apply_workspace_state(struct sway_workspace *ws,
		struct sway_workspace_state *state) {
	output_damage_whole(ws->current.output);
	list_free(ws->current.floating);
	list_free(ws->current.tiling);
	memcpy(&ws->current, state, sizeof(struct sway_workspace_state));
	output_damage_whole(ws->current.output);
}

static void apply_container_state(struct sway_container *container,
		struct sway_container_state *state) {
	struct sway_view *view = container->view;
	// Damage the old location
	desktop_damage_whole_container(container);
	if (view && view->saved_buffer) {
		struct wlr_box box = {
			.x = container->current.content_x - view->saved_geometry.x,
			.y = container->current.content_y - view->saved_geometry.y,
			.width = view->saved_buffer_width,
			.height = view->saved_buffer_height,
		};
		desktop_damage_box(&box);
	}

	// There are separate children lists for each instruction state, the
	// container's current state and the container's pending state
	// (ie. con->children). The list itself needs to be freed here.
	// Any child containers which are being deleted will be cleaned up in
	// transaction_destroy().
	list_free(container->current.children);

	memcpy(&container->current, state, sizeof(struct sway_container_state));

	if (view && view->saved_buffer) {
		if (!container->node.destroying || container->node.ntxnrefs == 1) {
			view_remove_saved_buffer(view);
		}
	}

	// Damage the new location
	desktop_damage_whole_container(container);
	if (view && view->surface) {
		struct wlr_surface *surface = view->surface;
		struct wlr_box box = {
			.x = container->current.content_x - view->geometry.x,
			.y = container->current.content_y - view->geometry.y,
			.width = surface->current.width,
			.height = surface->current.height,
		};
		desktop_damage_box(&box);
	}

	// If the view hasn't responded to the configure, center it within
	// the container. This is important for fullscreen views which
	// refuse to resize to the size of the output.
	if (view && view->surface) {
		if (view->surface->current.width < container->width) {
			container->surface_x = container->content_x +
				(container->content_width - view->surface->current.width) / 2;
		} else {
			container->surface_x = container->content_x;
		}
		if (view->surface->current.height < container->height) {
			container->surface_y = container->content_y +
				(container->content_height - view->surface->current.height) / 2;
		} else {
			container->surface_y = container->content_y;
		}
		container->surface_width = view->surface->current.width;
		container->surface_height = view->surface->current.height;
	}

	if (!container->node.destroying) {
		container_discover_outputs(container);
	}
}

/**
 * Apply a transaction to the "current" state of the tree.
 */
static void transaction_apply(struct sway_transaction *transaction) {
	sway_log(SWAY_DEBUG, "Applying transaction %p", transaction);
	if (debug.txn_timings) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		struct timespec *commit = &transaction->commit_time;
		float ms = (now.tv_sec - commit->tv_sec) * 1000 +
			(now.tv_nsec - commit->tv_nsec) / 1000000.0;
		sway_log(SWAY_DEBUG, "Transaction %p: %.1fms waiting "
				"(%.1f frames if 60Hz)", transaction, ms, ms / (1000.0f / 60));
	}

	// Apply the instruction state to the node's current state
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_node *node = instruction->node;

		switch (node->type) {
		case N_ROOT:
			break;
		case N_OUTPUT:
			apply_output_state(node->sway_output, &instruction->output_state);
			break;
		case N_WORKSPACE:
			apply_workspace_state(node->sway_workspace,
					&instruction->workspace_state);
			break;
		case N_CONTAINER:
			apply_container_state(node->sway_container,
					&instruction->container_state);
			break;
		}

		node->instruction = NULL;
	}

	if (root->outputs->length) {
		struct sway_seat *seat;
		wl_list_for_each(seat, &server.input->seats, link) {
			if (!seat_doing_seatop(seat)) {
				cursor_rebase(seat->cursor);
			}
		}
	}
}

static void transaction_commit(struct sway_transaction *transaction);

// Return true if both transactions operate on the same nodes
static bool transaction_same_nodes(struct sway_transaction *a,
		struct sway_transaction *b) {
	if (a->instructions->length != b->instructions->length) {
		return false;
	}
	for (int i = 0; i < a->instructions->length; ++i) {
		struct sway_transaction_instruction *a_inst = a->instructions->items[i];
		struct sway_transaction_instruction *b_inst = b->instructions->items[i];
		if (a_inst->node != b_inst->node) {
			return false;
		}
	}
	return true;
}

static void transaction_progress_queue(void) {
	if (!server.transactions->length) {
		return;
	}
	// There's only ever one committed transaction,
	// and it's the first one in the queue.
	struct sway_transaction *transaction = server.transactions->items[0];
	if (transaction->num_waiting) {
		return;
	}
	transaction_apply(transaction);
	transaction_destroy(transaction);
	list_del(server.transactions, 0);

	if (!server.transactions->length) {
		idle_inhibit_v1_check_active(server.idle_inhibit_manager_v1);
		return;
	}

	// If there's a bunch of consecutive transactions which all apply to the
	// same views, skip all except the last one.
	while (server.transactions->length >= 2) {
		struct sway_transaction *a = server.transactions->items[0];
		struct sway_transaction *b = server.transactions->items[1];
		if (transaction_same_nodes(a, b)) {
			list_del(server.transactions, 0);
			transaction_destroy(a);
		} else {
			break;
		}
	}

	transaction = server.transactions->items[0];
	transaction_commit(transaction);
	transaction_progress_queue();
}

static int handle_timeout(void *data) {
	struct sway_transaction *transaction = data;
	sway_log(SWAY_DEBUG, "Transaction %p timed out (%zi waiting)",
			transaction, transaction->num_waiting);
	transaction->num_waiting = 0;
	transaction_progress_queue();
	return 0;
}

static bool should_configure(struct sway_node *node,
		struct sway_transaction_instruction *instruction) {
	if (!node_is_view(node)) {
		return false;
	}
	if (node->destroying) {
		return false;
	}
	struct sway_container_state *cstate = &node->sway_container->current;
	struct sway_container_state *istate = &instruction->container_state;
#if HAVE_XWAYLAND
	// Xwayland views are position-aware and need to be reconfigured
	// when their position changes.
	if (node->sway_container->view->type == SWAY_VIEW_XWAYLAND) {
		if (cstate->content_x != istate->content_x ||
				cstate->content_y != istate->content_y) {
			return true;
		}
	}
#endif
	if (cstate->content_width == istate->content_width &&
			cstate->content_height == istate->content_height) {
		return false;
	}
	return true;
}

static void transaction_commit(struct sway_transaction *transaction) {
	sway_log(SWAY_DEBUG, "Transaction %p committing with %i instructions",
			transaction, transaction->instructions->length);
	transaction->num_waiting = 0;
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_node *node = instruction->node;
		if (should_configure(node, instruction)) {
			instruction->serial = view_configure(node->sway_container->view,
					instruction->container_state.content_x,
					instruction->container_state.content_y,
					instruction->container_state.content_width,
					instruction->container_state.content_height);
			++transaction->num_waiting;

			// From here on we are rendering a saved buffer of the view, which
			// means we can send a frame done event to make the client redraw it
			// as soon as possible. Additionally, this is required if a view is
			// mapping and its default geometry doesn't intersect an output.
			struct timespec when;
			wlr_surface_send_frame_done(
					node->sway_container->view->surface, &when);
		}
		if (node_is_view(node) && !node->sway_container->view->saved_buffer) {
			view_save_buffer(node->sway_container->view);
			memcpy(&node->sway_container->view->saved_geometry,
					&node->sway_container->view->geometry,
					sizeof(struct wlr_box));
		}
		node->instruction = instruction;
	}
	transaction->num_configures = transaction->num_waiting;
	if (debug.txn_timings) {
		clock_gettime(CLOCK_MONOTONIC, &transaction->commit_time);
	}
	if (debug.noatomic) {
		transaction->num_waiting = 0;
	} else if (debug.txn_wait) {
		// Force the transaction to time out even if all views are ready.
		// We do this by inflating the waiting counter.
		transaction->num_waiting += 1000000;
	}

	if (transaction->num_waiting) {
		// Set up a timer which the views must respond within
		transaction->timer = wl_event_loop_add_timer(server.wl_event_loop,
				handle_timeout, transaction);
		if (transaction->timer) {
			wl_event_source_timer_update(transaction->timer,
					server.txn_timeout_ms);
		} else {
			sway_log_errno(SWAY_ERROR, "Unable to create transaction timer "
					"(some imperfect frames might be rendered)");
			transaction->num_waiting = 0;
		}
	}

	// The debug tree shows the pending/live tree. Here is a good place to
	// update it, because we make a transaction every time we change the pending
	// tree.
	update_debug_tree();
}

static void set_instruction_ready(
		struct sway_transaction_instruction *instruction) {
	struct sway_transaction *transaction = instruction->transaction;

	if (debug.txn_timings) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		struct timespec *start = &transaction->commit_time;
		float ms = (now.tv_sec - start->tv_sec) * 1000 +
			(now.tv_nsec - start->tv_nsec) / 1000000.0;
		sway_log(SWAY_DEBUG, "Transaction %p: %zi/%zi ready in %.1fms (%s)",
				transaction,
				transaction->num_configures - transaction->num_waiting + 1,
				transaction->num_configures, ms,
				instruction->node->sway_container->title);
	}

	// If the transaction has timed out then its num_waiting will be 0 already.
	if (transaction->num_waiting > 0 && --transaction->num_waiting == 0) {
		sway_log(SWAY_DEBUG, "Transaction %p is ready", transaction);
		wl_event_source_timer_update(transaction->timer, 0);
	}

	instruction->node->instruction = NULL;
	transaction_progress_queue();
}

void transaction_notify_view_ready_by_serial(struct sway_view *view,
		uint32_t serial) {
	struct sway_transaction_instruction *instruction =
		view->container->node.instruction;
	if (instruction->serial == serial) {
		set_instruction_ready(instruction);
	}
}

void transaction_notify_view_ready_by_size(struct sway_view *view,
		int width, int height) {
	struct sway_transaction_instruction *instruction =
		view->container->node.instruction;
	if (instruction->container_state.content_width == width &&
			instruction->container_state.content_height == height) {
		set_instruction_ready(instruction);
	}
}

void transaction_commit_dirty(void) {
	if (!server.dirty_nodes->length) {
		return;
	}
	struct sway_transaction *transaction = transaction_create();
	if (!transaction) {
		return;
	}
	for (int i = 0; i < server.dirty_nodes->length; ++i) {
		struct sway_node *node = server.dirty_nodes->items[i];
		transaction_add_node(transaction, node);
		node->dirty = false;
	}
	server.dirty_nodes->length = 0;

	list_add(server.transactions, transaction);

	// There's only ever one committed transaction,
	// and it's the first one in the queue.
	if (server.transactions->length == 1) {
		transaction_commit(transaction);
		// Attempting to progress the queue here is useful
		// if the transaction has nothing to wait for.
		transaction_progress_queue();
	}
}
