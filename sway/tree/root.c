#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output_layout.h>
#include "sway/input/seat.h"
#include "sway/output.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/workspace.h"
#include "list.h"
#include "log.h"
#include "util.h"

struct sway_container root_container;

static void output_layout_handle_change(struct wl_listener *listener,
		void *data) {
	arrange_windows(&root_container);
	transaction_commit_dirty();
}

void root_create(void) {
	root_container.id = 0; // normally assigned in new_swayc()
	root_container.type = C_ROOT;
	root_container.layout = L_NONE;
	root_container.name = strdup("root");
	root_container.instructions = create_list();
	root_container.children = create_list();
	root_container.current.children = create_list();
	wl_signal_init(&root_container.events.destroy);

	root_container.sway_root = calloc(1, sizeof(*root_container.sway_root));
	root_container.sway_root->output_layout = wlr_output_layout_create();
	wl_list_init(&root_container.sway_root->outputs);
#ifdef HAVE_XWAYLAND
	wl_list_init(&root_container.sway_root->xwayland_unmanaged);
#endif
	wl_list_init(&root_container.sway_root->drag_icons);
	wl_signal_init(&root_container.sway_root->events.new_container);
	root_container.sway_root->scratchpad = create_list();

	root_container.sway_root->output_layout_change.notify =
		output_layout_handle_change;
	wl_signal_add(&root_container.sway_root->output_layout->events.change,
		&root_container.sway_root->output_layout_change);
}

void root_destroy(void) {
	// sway_root
	wl_list_remove(&root_container.sway_root->output_layout_change.link);
	list_free(root_container.sway_root->scratchpad);
	wlr_output_layout_destroy(root_container.sway_root->output_layout);
	free(root_container.sway_root);

	// root_container
	list_free(root_container.instructions);
	list_free(root_container.children);
	list_free(root_container.current.children);
	free(root_container.name);

	memset(&root_container, 0, sizeof(root_container));
}

void root_scratchpad_add_container(struct sway_container *con) {
	if (!sway_assert(!con->scratchpad, "Container is already in scratchpad")) {
		return;
	}
	con->scratchpad = true;
	list_add(root_container.sway_root->scratchpad, con);

	struct sway_container *parent = con->parent;
	container_set_floating(con, true);
	container_remove_child(con);
	arrange_windows(parent);

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	seat_set_focus(seat, seat_get_focus_inactive(seat, parent));
}

void root_scratchpad_remove_container(struct sway_container *con) {
	if (!sway_assert(con->scratchpad, "Container is not in scratchpad")) {
		return;
	}
	con->scratchpad = false;
	for (int i = 0; i < root_container.sway_root->scratchpad->length; ++i) {
		if (root_container.sway_root->scratchpad->items[i] == con) {
			list_del(root_container.sway_root->scratchpad, i);
			break;
		}
	}
}

void root_scratchpad_show(struct sway_container *con) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *ws = seat_get_focus(seat);
	if (ws->type != C_WORKSPACE) {
		ws = container_parent(ws, C_WORKSPACE);
	}

    // If the current con or any of its parents are in fullscreen mode, we
    // first need to disable it before showing the scratchpad con.
	if (ws->sway_workspace->fullscreen) {
		container_set_fullscreen(ws->sway_workspace->fullscreen, false);
	}

	// Show the container
	if (con->parent) {
		container_remove_child(con);
	}
	container_add_child(ws->sway_workspace->floating, con);

	// Make sure the container's center point overlaps this workspace
	double center_lx = con->x + con->width / 2;
	double center_ly = con->y + con->height / 2;

	struct wlr_box workspace_box;
	container_get_box(ws, &workspace_box);
	if (!wlr_box_contains_point(&workspace_box, center_lx, center_ly)) {
		// Maybe resize it
		if (con->width > ws->width || con->height > ws->height) {
			container_init_floating(con);
		}

		// Center it
		double new_lx = ws->x + (ws->width - con->width) / 2;
		double new_ly = ws->y + (ws->height - con->height) / 2;
		container_floating_move_to(con, new_lx, new_ly);
	}

	arrange_windows(ws);
	seat_set_focus(seat, seat_get_focus_inactive(seat, con));

	container_set_dirty(con->parent);
}

void root_scratchpad_hide(struct sway_container *con) {
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus = seat_get_focus(seat);
	struct sway_container *ws = container_parent(con, C_WORKSPACE);

	container_remove_child(con);
	arrange_windows(ws);
	if (con == focus) {
		seat_set_focus(seat, seat_get_focus_inactive(seat, ws));
	}
	list_move_to_end(root_container.sway_root->scratchpad, con);
}

struct pid_workspace {
	pid_t pid;
	char *workspace;
	struct timespec time_added;

	struct sway_container *output;
	struct wl_listener output_destroy;

	struct wl_list link;
};

static struct wl_list pid_workspaces;

struct sway_container *root_workspace_for_pid(pid_t pid) {
	if (!pid_workspaces.prev && !pid_workspaces.next) {
		wl_list_init(&pid_workspaces);
		return NULL;
	}

	struct sway_container *ws = NULL;
	struct pid_workspace *pw = NULL;

	wlr_log(WLR_DEBUG, "Looking up workspace for pid %d", pid);

	do {
		struct pid_workspace *_pw = NULL;
		wl_list_for_each(_pw, &pid_workspaces, link) {
			if (pid == _pw->pid) {
				pw = _pw;
				wlr_log(WLR_DEBUG,
						"found pid_workspace for pid %d, workspace %s",
						pid, pw->workspace);
				goto found;
			}
		}
		pid = get_parent_pid(pid);
	} while (pid > 1);
found:

	if (pw && pw->workspace) {
		ws = workspace_by_name(pw->workspace);

		if (!ws) {
			wlr_log(WLR_DEBUG,
					"Creating workspace %s for pid %d because it disappeared",
					pw->workspace, pid);
			ws = workspace_create(pw->output, pw->workspace);
		}

		wl_list_remove(&pw->output_destroy.link);
		wl_list_remove(&pw->link);
		free(pw->workspace);
		free(pw);
	}

	return ws;
}

static void pw_handle_output_destroy(struct wl_listener *listener, void *data) {
	struct pid_workspace *pw = wl_container_of(listener, pw, output_destroy);
	pw->output = NULL;
	wl_list_remove(&pw->output_destroy.link);
	wl_list_init(&pw->output_destroy.link);
}

void root_record_workspace_pid(pid_t pid) {
	wlr_log(WLR_DEBUG, "Recording workspace for process %d", pid);
	if (!pid_workspaces.prev && !pid_workspaces.next) {
		wl_list_init(&pid_workspaces);
	}

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *ws =
		seat_get_focus_inactive(seat, &root_container);
	if (ws && ws->type != C_WORKSPACE) {
		ws = container_parent(ws, C_WORKSPACE);
	}
	if (!ws) {
		wlr_log(WLR_DEBUG, "Bailing out, no workspace");
		return;
	}
	struct sway_container *output = ws->parent;
	if (!output) {
		wlr_log(WLR_DEBUG, "Bailing out, no output");
		return;
	}

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	// Remove expired entries
	static const int timeout = 60;
	struct pid_workspace *old, *_old;
	wl_list_for_each_safe(old, _old, &pid_workspaces, link) {
		if (now.tv_sec - old->time_added.tv_sec >= timeout) {
			wl_list_remove(&old->output_destroy.link);
			wl_list_remove(&old->link);
			free(old->workspace);
			free(old);
		}
	}

	struct pid_workspace *pw = calloc(1, sizeof(struct pid_workspace));
	pw->workspace = strdup(ws->name);
	pw->output = output;
	pw->pid = pid;
	memcpy(&pw->time_added, &now, sizeof(struct timespec));
	pw->output_destroy.notify = pw_handle_output_destroy;
	wl_signal_add(&output->sway_output->wlr_output->events.destroy,
			&pw->output_destroy);
	wl_list_insert(&pid_workspaces, &pw->link);
}
