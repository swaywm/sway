#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include "log.h"
#include "sway/input/keyboard.h"
#include "sway/input/seat.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/surface.h"

struct sway_session_lock_surface {
	struct wlr_session_lock_surface_v1 *lock_surface;
	struct sway_output *output;
	struct wlr_surface *surface;
	struct wl_listener map;
	struct wl_listener destroy;
	struct wl_listener surface_commit;
	struct wl_listener output_commit;
	struct wl_listener output_destroy;
};

static void set_lock_focused_surface(struct wlr_surface *focused) {
	server.session_lock.focused = focused;

	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		seat_set_focus_surface(seat, focused, false);
	}
}

static void handle_surface_map(struct wl_listener *listener, void *data) {
	struct sway_session_lock_surface *surf = wl_container_of(listener, surf, map);
	if (server.session_lock.focused == NULL) {
		set_lock_focused_surface(surf->surface);
	}
	surface_enter_output(surf->surface, surf->output);
	output_damage_whole(surf->output);
}

static void handle_surface_commit(struct wl_listener *listener, void *data) {
	struct sway_session_lock_surface *surf = wl_container_of(listener, surf, surface_commit);
	output_damage_surface(surf->output, 0, 0, surf->surface, false);
}

static void handle_output_commit(struct wl_listener *listener, void *data) {
	struct wlr_output_event_commit *event = data;
	struct sway_session_lock_surface *surf = wl_container_of(listener, surf, output_commit);
	if (event->committed & (
			WLR_OUTPUT_STATE_MODE |
			WLR_OUTPUT_STATE_SCALE |
			WLR_OUTPUT_STATE_TRANSFORM)) {
		wlr_session_lock_surface_v1_configure(surf->lock_surface,
			surf->output->width, surf->output->height);
	}
}

static void destroy_lock_surface(struct sway_session_lock_surface *surf) {
	// Move the seat focus to another surface if one is available
	if (server.session_lock.focused == surf->surface) {
		struct wlr_surface *next_focus = NULL;

		struct wlr_session_lock_surface_v1 *other;
		wl_list_for_each(other, &server.session_lock.lock->surfaces, link) {
			if (other != surf->lock_surface && other->mapped) {
				next_focus = other->surface;
				break;
			}
		}
		set_lock_focused_surface(next_focus);
	}

	wl_list_remove(&surf->map.link);
	wl_list_remove(&surf->destroy.link);
	wl_list_remove(&surf->surface_commit.link);
	wl_list_remove(&surf->output_commit.link);
	wl_list_remove(&surf->output_destroy.link);
	output_damage_whole(surf->output);
	free(surf);
}

static void handle_surface_destroy(struct wl_listener *listener, void *data) {
	struct sway_session_lock_surface *surf = wl_container_of(listener, surf, destroy);
	destroy_lock_surface(surf);
}

static void handle_output_destroy(struct wl_listener *listener, void *data) {
	struct sway_session_lock_surface *surf =
		wl_container_of(listener, surf, output_destroy);
	destroy_lock_surface(surf);
}

static void handle_new_surface(struct wl_listener *listener, void *data) {
	struct wlr_session_lock_surface_v1 *lock_surface = data;
	struct sway_session_lock_surface *surf = calloc(1, sizeof(*surf));
	if (surf == NULL) {
		return;
	}

	sway_log(SWAY_DEBUG, "new lock layer surface");

	struct sway_output *output = lock_surface->output->data;
	wlr_session_lock_surface_v1_configure(lock_surface, output->width, output->height);

	surf->lock_surface = lock_surface;
	surf->surface = lock_surface->surface;
	surf->output = output;
	surf->map.notify = handle_surface_map;
	wl_signal_add(&lock_surface->events.map, &surf->map);
	surf->destroy.notify = handle_surface_destroy;
	wl_signal_add(&lock_surface->events.destroy, &surf->destroy);
	surf->surface_commit.notify = handle_surface_commit;
	wl_signal_add(&surf->surface->events.commit, &surf->surface_commit);
	surf->output_commit.notify = handle_output_commit;
	wl_signal_add(&output->wlr_output->events.commit, &surf->output_commit);
	surf->output_destroy.notify = handle_output_destroy;
	wl_signal_add(&output->node.events.destroy, &surf->output_destroy);
}

static void handle_unlock(struct wl_listener *listener, void *data) {
	sway_log(SWAY_DEBUG, "session unlocked");
	server.session_lock.locked = false;
	server.session_lock.lock = NULL;
	server.session_lock.focused = NULL;

	wl_list_remove(&server.session_lock.lock_new_surface.link);
	wl_list_remove(&server.session_lock.lock_unlock.link);
	wl_list_remove(&server.session_lock.lock_destroy.link);

	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		seat_set_exclusive_client(seat, NULL);
		// copied from seat_set_focus_layer -- deduplicate?
		struct sway_node *previous = seat_get_focus_inactive(seat, &root->node);
		if (previous) {
			// Hack to get seat to re-focus the return value of get_focus
			seat_set_focus(seat, NULL);
			seat_set_focus(seat, previous);
		}
	}

	// redraw everything
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		output_damage_whole(output);
	}
}

static void handle_abandon(struct wl_listener *listener, void *data) {
	sway_log(SWAY_INFO, "session lock abandoned");
	server.session_lock.lock = NULL;
	server.session_lock.focused = NULL;

	wl_list_remove(&server.session_lock.lock_new_surface.link);
	wl_list_remove(&server.session_lock.lock_unlock.link);
	wl_list_remove(&server.session_lock.lock_destroy.link);

	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		seat->exclusive_client = NULL;
	}

	// redraw everything
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		output_damage_whole(output);
	}
}

static void handle_session_lock(struct wl_listener *listener, void *data) {
	struct wlr_session_lock_v1 *lock = data;
	struct wl_client *client = wl_resource_get_client(lock->resource);

	if (server.session_lock.lock) {
		wlr_session_lock_v1_destroy(lock);
		return;
	}

	sway_log(SWAY_DEBUG, "session locked");
	server.session_lock.locked = true;
	server.session_lock.lock = lock;

	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		seat_set_exclusive_client(seat, client);
	}

	wl_signal_add(&lock->events.new_surface, &server.session_lock.lock_new_surface);
	wl_signal_add(&lock->events.unlock, &server.session_lock.lock_unlock);
	wl_signal_add(&lock->events.destroy, &server.session_lock.lock_destroy);

	wlr_session_lock_v1_send_locked(lock);

	// redraw everything
	for (int i = 0; i < root->outputs->length; ++i) {
		struct sway_output *output = root->outputs->items[i];
		output_damage_whole(output);
	}
}

static void handle_session_lock_destroy(struct wl_listener *listener, void *data) {
	assert(server.session_lock.lock == NULL);
	wl_list_remove(&server.session_lock.new_lock.link);
	wl_list_remove(&server.session_lock.manager_destroy.link);
}

void sway_session_lock_init(void) {
	server.session_lock.manager = wlr_session_lock_manager_v1_create(server.wl_display);

	server.session_lock.lock_new_surface.notify = handle_new_surface;
	server.session_lock.lock_unlock.notify = handle_unlock;
	server.session_lock.lock_destroy.notify = handle_abandon;
	server.session_lock.new_lock.notify = handle_session_lock;
	server.session_lock.manager_destroy.notify = handle_session_lock_destroy;
	wl_signal_add(&server.session_lock.manager->events.new_lock,
		&server.session_lock.new_lock);
	wl_signal_add(&server.session_lock.manager->events.destroy,
		&server.session_lock.manager_destroy);
}
