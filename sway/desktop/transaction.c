#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wlr/types/wlr_buffer.h>
#include "sway/debug.h"
#include "sway/desktop/idle_inhibit_v1.h"
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
int txn_timeout_ms = 200;

/**
 * If enabled, sway will always wait for the transaction timeout before
 * applying it, rather than applying it when the views are ready. This allows us
 * to observe the rendered state while a transaction is in progress.
 */
bool txn_debug = false;

struct sway_transaction {
	struct wl_event_source *timer;
	list_t *instructions;   // struct sway_transaction_instruction *
	size_t num_waiting;
	size_t num_configures;
	uint32_t con_ids;       // Bitwise XOR of view container IDs
	struct timespec create_time;
	struct timespec commit_time;
};

struct sway_transaction_instruction {
	struct sway_transaction *transaction;
	struct sway_container *container;
	struct sway_container_state state;
	uint32_t serial;
};

static struct sway_transaction *transaction_create() {
	struct sway_transaction *transaction =
		calloc(1, sizeof(struct sway_transaction));
	if (!sway_assert(transaction, "Unable to allocate transaction")) {
		return NULL;
	}
	transaction->instructions = create_list();
	if (server.debug_txn_timings) {
		clock_gettime(CLOCK_MONOTONIC, &transaction->create_time);
	}
	return transaction;
}

static void transaction_destroy(struct sway_transaction *transaction) {
	// Free instructions
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_container *con = instruction->container;
		con->ntxnrefs--;
		if (con->instruction == instruction) {
			con->instruction = NULL;
		}
		if (con->destroying && con->ntxnrefs == 0) {
			container_free(con);
		}
		free(instruction);
	}
	list_free(transaction->instructions);

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
	state->is_fullscreen = container->is_fullscreen;
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
		state->border = view->border;
		state->border_thickness = view->border_thickness;
		state->border_top = view->border_top;
		state->border_left = view->border_left;
		state->border_right = view->border_right;
		state->border_bottom = view->border_bottom;
		state->using_csd = view->using_csd;
	} else if (container->type == C_WORKSPACE) {
		state->ws_fullscreen = container->sway_workspace->fullscreen;
		state->ws_floating = container->sway_workspace->floating;
		state->children = create_list();
		list_cat(state->children, container->children);
	} else {
		state->children = create_list();
		list_cat(state->children, container->children);
	}

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	state->focused = seat_get_focus(seat) == container;

	if (container->type != C_VIEW) {
		state->focused_inactive_child =
			seat_get_active_child(seat, container);
	}
}

static void transaction_add_container(struct sway_transaction *transaction,
		struct sway_container *container) {
	struct sway_transaction_instruction *instruction =
		calloc(1, sizeof(struct sway_transaction_instruction));
	if (!sway_assert(instruction, "Unable to allocate instruction")) {
		return;
	}
	instruction->transaction = transaction;
	instruction->container = container;

	copy_pending_state(container, &instruction->state);

	list_add(transaction->instructions, instruction);
	container->ntxnrefs++;
}

/**
 * Apply a transaction to the "current" state of the tree.
 */
static void transaction_apply(struct sway_transaction *transaction) {
	wlr_log(WLR_DEBUG, "Applying transaction %p", transaction);
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
		wlr_log(WLR_DEBUG, "Transaction %p: %.1fms arranging, %.1fms waiting, "
			"%.1fms total (%.1f frames if 60Hz)", transaction,
			ms_arranging, ms_waiting, ms_total, ms_total / (1000.0f / 60));
	}

	// Apply the instruction state to the container's current state
	for (int i = 0; i < transaction->instructions->length; ++i) {
		struct sway_transaction_instruction *instruction =
			transaction->instructions->items[i];
		struct sway_container *container = instruction->container;

		// Damage the old and new locations
		struct wlr_box old_con_box = {
			.x = container->current.swayc_x,
			.y = container->current.swayc_y,
			.width = container->current.swayc_width,
			.height = container->current.swayc_height,
		};
		struct wlr_box new_con_box = {
			.x = instruction->state.swayc_x,
			.y = instruction->state.swayc_y,
			.width = instruction->state.swayc_width,
			.height = instruction->state.swayc_height,
		};
		// Handle geometry, which may overflow the bounds of the container
		struct wlr_box old_surface_box = {0,0,0,0};
		struct wlr_box new_surface_box = {0,0,0,0};
		if (container->type == C_VIEW) {
			struct sway_view *view = container->sway_view;
			if (container->sway_view->saved_buffer) {
				old_surface_box.x =
					container->current.view_x - view->saved_geometry.x;
				old_surface_box.y =
					container->current.view_y - view->saved_geometry.y;
				old_surface_box.width = view->saved_buffer_width;
				old_surface_box.height = view->saved_buffer_height;
			}
			struct wlr_surface *surface = container->sway_view->surface;
			if (surface) {
				struct wlr_box geometry;
				view_get_geometry(view, &geometry);
				new_surface_box.x = instruction->state.view_x - geometry.x;
				new_surface_box.y = instruction->state.view_y - geometry.y;
				new_surface_box.width = surface->current.width;
				new_surface_box.height = surface->current.height;
			}
		}
		for (int j = 0; j < root_container.current.children->length; ++j) {
			struct sway_container *output =
				root_container.current.children->items[j];
			if (output->sway_output) {
				output_damage_box(output->sway_output, &old_con_box);
				output_damage_box(output->sway_output, &new_con_box);
				output_damage_box(output->sway_output, &old_surface_box);
				output_damage_box(output->sway_output, &new_surface_box);
			}
		}

		// There are separate children lists for each instruction state, the
		// container's current state and the container's pending state
		// (ie. con->children). The list itself needs to be freed here.
		// Any child containers which are being deleted will be cleaned up in
		// transaction_destroy().
		list_free(container->current.children);

		memcpy(&container->current, &instruction->state,
				sizeof(struct sway_container_state));

		if (container->type == C_VIEW && container->sway_view->saved_buffer) {
			view_remove_saved_buffer(container->sway_view);
		}

		container->instruction = NULL;
	}
}

static void transaction_commit(struct sway_transaction *transaction);

static void transaction_progress_queue() {
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
		if (a->con_ids == b->con_ids) {
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
	wlr_log(WLR_DEBUG, "Transaction %p timed out (%li waiting)",
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
	if (con->current.view_width == instruction->state.view_width &&
			con->current.view_height == instruction->state.view_height) {
		return false;
	}
	return true;
}

static void transaction_commit(struct sway_transaction *transaction) {
	wlr_log(WLR_DEBUG, "Transaction %p committing with %i instructions",
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
			transaction->con_ids ^= con->id;

			// From here on we are rendering a saved buffer of the view, which
			// means we can send a frame done event to make the client redraw it
			// as soon as possible. Additionally, this is required if a view is
			// mapping and its default geometry doesn't intersect an output.
			struct timespec when;
			wlr_surface_send_frame_done(con->sway_view->surface, &when);
		}
		if (con->type == C_VIEW) {
			view_save_buffer(con->sway_view);
			view_get_geometry(con->sway_view, &con->sway_view->saved_geometry);
		}
		con->instruction = instruction;
	}
	transaction->num_configures = transaction->num_waiting;
	if (server.debug_txn_timings) {
		clock_gettime(CLOCK_MONOTONIC, &transaction->commit_time);
	}

	if (transaction->num_waiting) {
		// Set up a timer which the views must respond within
		transaction->timer = wl_event_loop_add_timer(server.wl_event_loop,
				handle_timeout, transaction);
		if (transaction->timer) {
			wl_event_source_timer_update(transaction->timer, txn_timeout_ms);
		} else {
			wlr_log(WLR_ERROR, "Unable to create transaction timer (%s). "
					"Some imperfect frames might be rendered.",
					strerror(errno));
			handle_timeout(transaction);
		}
	} else {
		wlr_log(WLR_DEBUG,
				"Transaction %p has nothing to wait for", transaction);
	}

	// The debug tree shows the pending/live tree. Here is a good place to
	// update it, because we make a transaction every time we change the pending
	// tree.
	update_debug_tree();
}

static void set_instruction_ready(
		struct sway_transaction_instruction *instruction) {
	struct sway_transaction *transaction = instruction->transaction;

	if (server.debug_txn_timings) {
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		struct timespec *start = &transaction->commit_time;
		float ms = (now.tv_sec - start->tv_sec) * 1000 +
			(now.tv_nsec - start->tv_nsec) / 1000000.0;
		wlr_log(WLR_DEBUG, "Transaction %p: %li/%li ready in %.1fms (%s)",
				transaction,
				transaction->num_configures - transaction->num_waiting + 1,
				transaction->num_configures, ms,
				instruction->container->name);

	}

	// If the transaction has timed out then its num_waiting will be 0 already.
	if (transaction->num_waiting > 0 && --transaction->num_waiting == 0) {
		if (!txn_debug) {
			wlr_log(WLR_DEBUG, "Transaction %p is ready", transaction);
			wl_event_source_timer_update(transaction->timer, 0);
		}
	}

	instruction->container->instruction = NULL;
	if (!txn_debug) {
		transaction_progress_queue();
	}
}

void transaction_notify_view_ready_by_serial(struct sway_view *view,
		uint32_t serial) {
	struct sway_transaction_instruction *instruction = view->swayc->instruction;
	if (view->swayc->instruction->serial == serial) {
		set_instruction_ready(instruction);
	}
}

void transaction_notify_view_ready_by_size(struct sway_view *view,
		int width, int height) {
	struct sway_transaction_instruction *instruction = view->swayc->instruction;
	if (instruction->state.view_width == width &&
			instruction->state.view_height == height) {
		set_instruction_ready(instruction);
	}
}

void transaction_commit_dirty(void) {
	if (!server.dirty_containers->length) {
		return;
	}
	struct sway_transaction *transaction = transaction_create();
	if (!transaction) {
		return;
	}
	for (int i = 0; i < server.dirty_containers->length; ++i) {
		struct sway_container *container = server.dirty_containers->items[i];
		transaction_add_container(transaction, container);
		container->dirty = false;
	}
	server.dirty_containers->length = 0;

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
