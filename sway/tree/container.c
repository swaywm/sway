#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wayland-server.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_xdg_shell_v6.h>
#include <wlr/types/wlr_xdg_shell.h>
#include "cairo.h"
#include "pango.h"
#include "sway/config.h"
#include "sway/desktop.h"
#include "sway/desktop/transaction.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/layout.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "log.h"
#include "stringop.h"

static list_t *bfs_queue;

static list_t *get_bfs_queue() {
	if (!bfs_queue) {
		bfs_queue = create_list();
		if (!bfs_queue) {
			wlr_log(WLR_ERROR, "could not allocate list for bfs queue");
			return NULL;
		}
	}
	bfs_queue->length = 0;

	return bfs_queue;
}

const char *container_type_to_str(enum sway_container_type type) {
	switch (type) {
	case C_ROOT:
		return "C_ROOT";
	case C_OUTPUT:
		return "C_OUTPUT";
	case C_WORKSPACE:
		return "C_WORKSPACE";
	case C_CONTAINER:
		return "C_CONTAINER";
	case C_VIEW:
		return "C_VIEW";
	default:
		return "C_UNKNOWN";
	}
}

void container_create_notify(struct sway_container *container) {
	// TODO send ipc event type based on the container type
	wl_signal_emit(&root_container.sway_root->events.new_container, container);

	if (container->type == C_VIEW) {
		ipc_event_window(container, "new");
	} else if (container->type == C_WORKSPACE) {
		ipc_event_workspace(NULL, container, "init");
	}
}

void container_update_textures_recursive(struct sway_container *con) {
	if (con->type == C_CONTAINER || con->type == C_VIEW) {
		container_update_title_textures(con);
	}

	if (con->type == C_VIEW) {
		view_update_marks_textures(con->sway_view);
	} else {
		for (int i = 0; i < con->children->length; ++i) {
			struct sway_container *child = con->children->items[i];
			container_update_textures_recursive(child);
		}

		if (con->type == C_WORKSPACE) {
			container_update_textures_recursive(con->sway_workspace->floating);
		}
	}
}

static void handle_reparent(struct wl_listener *listener,
		void *data) {
	struct sway_container *container =
		wl_container_of(listener, container, reparent);
	struct sway_container *old_parent = data;

	struct sway_container *old_output = old_parent;
	if (old_output != NULL && old_output->type != C_OUTPUT) {
		old_output = container_parent(old_output, C_OUTPUT);
	}

	struct sway_container *new_output = container->parent;
	if (new_output != NULL && new_output->type != C_OUTPUT) {
		new_output = container_parent(new_output, C_OUTPUT);
	}

	if (old_output && new_output) {
		float old_scale = old_output->sway_output->wlr_output->scale;
		float new_scale = new_output->sway_output->wlr_output->scale;
		if (old_scale != new_scale) {
			container_update_textures_recursive(container);
		}
	}
}

struct sway_container *container_create(enum sway_container_type type) {
	// next id starts at 1 because 0 is assigned to root_container in layout.c
	static size_t next_id = 1;
	struct sway_container *c = calloc(1, sizeof(struct sway_container));
	if (!c) {
		return NULL;
	}
	c->id = next_id++;
	c->layout = L_NONE;
	c->type = type;
	c->alpha = 1.0f;
	c->instructions = create_list();

	if (type != C_VIEW) {
		c->children = create_list();
		c->current.children = create_list();
	}

	wl_signal_init(&c->events.destroy);
	wl_signal_init(&c->events.reparent);

	wl_signal_add(&c->events.reparent, &c->reparent);
	c->reparent.notify = handle_reparent;

	c->has_gaps = false;
	c->gaps_inner = 0;
	c->gaps_outer = 0;
	c->current_gaps = 0;

	return c;
}

static void container_workspace_free(struct sway_workspace *ws) {
	list_foreach(ws->output_priority, free);
	list_free(ws->output_priority);
	free(ws);
}

void container_free(struct sway_container *cont) {
	if (!sway_assert(cont->destroying,
				"Tried to free container which wasn't marked as destroying")) {
		return;
	}
	if (!sway_assert(cont->instructions->length == 0,
				"Tried to free container with pending instructions")) {
		return;
	}
	free(cont->name);
	free(cont->formatted_title);
	wlr_texture_destroy(cont->title_focused);
	wlr_texture_destroy(cont->title_focused_inactive);
	wlr_texture_destroy(cont->title_unfocused);
	wlr_texture_destroy(cont->title_urgent);
	list_free(cont->instructions);
	list_free(cont->children);
	list_free(cont->current.children);

	switch (cont->type) {
	case C_ROOT:
		break;
	case C_OUTPUT:
		break;
	case C_WORKSPACE:
		container_workspace_free(cont->sway_workspace);
		break;
	case C_CONTAINER:
		break;
	case C_VIEW:
		{
			struct sway_view *view = cont->sway_view;
			view->swayc = NULL;
			free(view->title_format);
			view->title_format = NULL;

			if (view->destroying) {
				view_free(view);
			}
		}
		break;
	case C_TYPES:
		sway_assert(false, "Didn't expect to see C_TYPES here");
		break;
	}

	free(cont);
}

static struct sway_container *container_destroy_noreaping(
		struct sway_container *con);

static struct sway_container *container_workspace_destroy(
		struct sway_container *workspace) {
	if (!sway_assert(workspace, "cannot destroy null workspace")) {
		return NULL;
	}

	struct sway_container *output = container_parent(workspace, C_OUTPUT);

	// If we're destroying the output, it will be NULL here. Return the root so
	// that it doesn't appear that the workspace has refused to be destoyed,
	// which would leave it in a broken state with no parent.
	if (output == NULL) {
		return &root_container;
	}

	// Do not destroy this if it's the last workspace on this output
	if (output->children->length == 1) {
		return NULL;
	}

	wlr_log(WLR_DEBUG, "destroying workspace '%s'", workspace->name);

	if (!workspace_is_empty(workspace)) {
		// Move children to a different workspace on this output
		struct sway_container *new_workspace = NULL;
		for (int i = 0; i < output->children->length; i++) {
			if (output->children->items[i] != workspace) {
				new_workspace = output->children->items[i];
				break;
			}
		}

		wlr_log(WLR_DEBUG, "moving children to different workspace '%s' -> '%s'",
			workspace->name, new_workspace->name);
		for (int i = 0; i < workspace->children->length; i++) {
			container_move_to(workspace->children->items[i], new_workspace);
		}
		struct sway_container *floating = workspace->sway_workspace->floating;
		for (int i = 0; i < floating->children->length; i++) {
			container_move_to(floating->children->items[i],
					new_workspace->sway_workspace->floating);
		}
	}

	container_destroy_noreaping(workspace->sway_workspace->floating);

	return output;
}

static struct sway_container *container_output_destroy(
		struct sway_container *output) {
	if (!sway_assert(output, "cannot destroy null output")) {
		return NULL;
	}

	if (output->children->length > 0) {
		// TODO save workspaces when there are no outputs.
		// TODO also check if there will ever be no outputs except for exiting
		// program
		if (root_container.children->length > 1) {
			// Move workspace from this output to another output
			struct sway_container *fallback_output =
				root_container.children->items[0];
			if (fallback_output == output) {
				fallback_output = root_container.children->items[1];
			}

			while (output->children->length) {
				struct sway_container *workspace = output->children->items[0];

				struct sway_container *new_output =
					workspace_output_get_highest_available(workspace, output);
				if (!new_output) {
					new_output = fallback_output;
					workspace_output_add_priority(workspace, new_output);
				}

				container_remove_child(workspace);
				if (!workspace_is_empty(workspace)) {
					container_add_child(new_output, workspace);
					ipc_event_workspace(NULL, workspace, "move");
				} else {
					container_destroy(workspace);
				}

				container_sort_workspaces(new_output);
			}
		}
	}

	wl_list_remove(&output->sway_output->mode.link);
	wl_list_remove(&output->sway_output->transform.link);
	wl_list_remove(&output->sway_output->scale.link);

	wl_list_remove(&output->sway_output->damage_destroy.link);
	wl_list_remove(&output->sway_output->damage_frame.link);

	output->sway_output->swayc = NULL;
	output->sway_output = NULL;

	wlr_log(WLR_DEBUG, "OUTPUT: Destroying output '%s'", output->name);

	return &root_container;
}

/**
 * Implement the actual destroy logic, without reaping.
 */
static struct sway_container *container_destroy_noreaping(
		struct sway_container *con) {
	if (con == NULL) {
		return NULL;
	}
	if (con->destroying) {
		return NULL;
	}

	wl_signal_emit(&con->events.destroy, con);

	// emit IPC event
	if (con->type == C_VIEW) {
		ipc_event_window(con, "close");
	} else if (con->type == C_WORKSPACE) {
		ipc_event_workspace(NULL, con, "empty");
	}

	// The below functions move their children to somewhere else.
	if (con->type == C_OUTPUT) {
		container_output_destroy(con);
	} else if (con->type == C_WORKSPACE) {
		// Workspaces will refuse to be destroyed if they're the last workspace
		// on their output.
		if (!container_workspace_destroy(con)) {
			return NULL;
		}
	}

	container_end_mouse_operation(con);

	con->destroying = true;
	container_set_dirty(con);

	if (con->scratchpad) {
		root_scratchpad_remove_container(con);
	}

	if (!con->parent) {
		return NULL;
	}

	return container_remove_child(con);
}

bool container_reap_empty(struct sway_container *con) {
	if (con->layout == L_FLOATING) {
		// Don't reap the magical floating container that each workspace has
		return false;
	}
	switch (con->type) {
	case C_ROOT:
	case C_OUTPUT:
		// dont reap these
		break;
	case C_WORKSPACE:
		if (!workspace_is_visible(con) && workspace_is_empty(con)) {
			wlr_log(WLR_DEBUG, "Destroying workspace via reaper");
			container_destroy_noreaping(con);
			return true;
		}
		break;
	case C_CONTAINER:
		if (con->children->length == 0) {
			container_destroy_noreaping(con);
			return true;
		}
	case C_VIEW:
		break;
	case C_TYPES:
		sway_assert(false, "container_reap_empty called on an invalid "
			"container");
		break;
	}

	return false;
}

struct sway_container *container_reap_empty_recursive(
		struct sway_container *con) {
	while (con) {
		struct sway_container *next = con->parent;
		if (!container_reap_empty(con)) {
			break;
		}
		con = next;
	}
	return con;
}

struct sway_container *container_flatten(struct sway_container *container) {
	while (container->type == C_CONTAINER && container->children->length == 1) {
		struct sway_container *child = container->children->items[0];
		struct sway_container *parent = container->parent;
		container_replace_child(container, child);
		container_destroy_noreaping(container);
		container = parent;
	}
	return container;
}

/**
 * container_destroy() is the first step in destroying a container. We'll emit
 * events, detach it from the tree and mark it as destroying. The container will
 * remain in memory until it's no longer used by a transaction, then it will be
 * freed via container_free().
 *
 * This function just wraps container_destroy_noreaping(), then does reaping.
 */
struct sway_container *container_destroy(struct sway_container *con) {
	if (con->is_fullscreen) {
		struct sway_container *ws = container_parent(con, C_WORKSPACE);
		ws->sway_workspace->fullscreen = NULL;
	}
	struct sway_container *parent = container_destroy_noreaping(con);

	if (!parent) {
		return NULL;
	}

	return container_reap_empty_recursive(parent);
}

static void container_close_func(struct sway_container *container, void *data) {
	if (container->type == C_VIEW) {
		view_close(container->sway_view);
	}
}

struct sway_container *container_close(struct sway_container *con) {
	if (!sway_assert(con != NULL,
			"container_close called with a NULL container")) {
		return NULL;
	}

	struct sway_container *parent = con->parent;

	if (con->type == C_VIEW) {
		view_close(con->sway_view);
	} else {
		container_for_each_descendant_dfs(con, container_close_func, NULL);
	}

	return parent;
}

struct sway_container *container_view_create(struct sway_container *sibling,
		struct sway_view *sway_view) {
	if (!sway_assert(sibling,
			"container_view_create called with NULL sibling/parent")) {
		return NULL;
	}
	const char *title = view_get_title(sway_view);
	struct sway_container *swayc = container_create(C_VIEW);
	wlr_log(WLR_DEBUG, "Adding new view %p:%s to container %p %d %s",
		swayc, title, sibling, sibling ? sibling->type : 0, sibling->name);
	// Setup values
	swayc->sway_view = sway_view;
	swayc->width = 0;
	swayc->height = 0;

	if (sibling->type == C_WORKSPACE) {
		// Case of focused workspace, just create as child of it
		container_add_child(sibling, swayc);
	} else {
		// Regular case, create as sibling of current container
		container_add_sibling(sibling, swayc);
	}
	container_create_notify(swayc);
	return swayc;
}

void container_descendants(struct sway_container *root,
		enum sway_container_type type,
		void (*func)(struct sway_container *item, void *data), void *data) {
	if (!root->children || !root->children->length) {
		return;
	}
	for (int i = 0; i < root->children->length; ++i) {
		struct sway_container *item = root->children->items[i];
		if (item->type == type) {
			func(item, data);
		}
		container_descendants(item, type, func, data);
	}
}

struct sway_container *container_find(struct sway_container *container,
		bool (*test)(struct sway_container *view, void *data), void *data) {
	if (!container->children) {
		return NULL;
	}
	for (int i = 0; i < container->children->length; ++i) {
		struct sway_container *child = container->children->items[i];
		if (test(child, data)) {
			return child;
		} else {
			struct sway_container *res = container_find(child, test, data);
			if (res) {
				return res;
			}
		}
	}
	if (container->type == C_WORKSPACE) {
		return container_find(container->sway_workspace->floating, test, data);
	}
	return NULL;
}

struct sway_container *container_parent(struct sway_container *container,
		enum sway_container_type type) {
	if (!sway_assert(container, "container is NULL")) {
		return NULL;
	}
	if (!sway_assert(type < C_TYPES && type >= C_ROOT, "invalid type")) {
		return NULL;
	}
	do {
		container = container->parent;
	} while (container && container->type != type);
	return container;
}

static struct sway_container *container_at_view(struct sway_container *swayc,
		double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	if (!sway_assert(swayc->type == C_VIEW, "Expected a view")) {
		return NULL;
	}
	struct sway_view *sview = swayc->sway_view;
	double view_sx = lx - sview->x;
	double view_sy = ly - sview->y;

	double _sx, _sy;
	struct wlr_surface *_surface = NULL;
	switch (sview->type) {
#ifdef HAVE_XWAYLAND
	case SWAY_VIEW_XWAYLAND:
		_surface = wlr_surface_surface_at(sview->surface,
				view_sx, view_sy, &_sx, &_sy);
		break;
#endif
	case SWAY_VIEW_XDG_SHELL_V6:
		_surface = wlr_xdg_surface_v6_surface_at(
				sview->wlr_xdg_surface_v6,
				view_sx, view_sy, &_sx, &_sy);
		break;
	case SWAY_VIEW_XDG_SHELL:
		_surface = wlr_xdg_surface_surface_at(
				sview->wlr_xdg_surface,
				view_sx, view_sy, &_sx, &_sy);
		break;
	}
	if (_surface) {
		*sx = _sx;
		*sy = _sy;
		*surface = _surface;
		return swayc;
	}
	return NULL;
}

/**
 * container_at for a container with layout L_TABBED.
 */
static struct sway_container *container_at_tabbed(struct sway_container *parent,
		double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	if (ly < parent->y || ly > parent->y + parent->height) {
		return NULL;
	}
	struct sway_seat *seat = input_manager_current_seat(input_manager);

	// Tab titles
	int title_height = container_titlebar_height();
	if (ly < parent->y + title_height) {
		int tab_width = parent->width / parent->children->length;
		int child_index = (lx - parent->x) / tab_width;
		if (child_index >= parent->children->length) {
			child_index = parent->children->length - 1;
		}
		struct sway_container *child = parent->children->items[child_index];
		return seat_get_focus_inactive(seat, child);
	}

	// Surfaces
	struct sway_container *current = seat_get_active_child(seat, parent);

	return tiling_container_at(current, lx, ly, surface, sx, sy);
}

/**
 * container_at for a container with layout L_STACKED.
 */
static struct sway_container *container_at_stacked(
		struct sway_container *parent, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	if (ly < parent->y || ly > parent->y + parent->height) {
		return NULL;
	}
	struct sway_seat *seat = input_manager_current_seat(input_manager);

	// Title bars
	int title_height = container_titlebar_height();
	int child_index = (ly - parent->y) / title_height;
	if (child_index < parent->children->length) {
		struct sway_container *child = parent->children->items[child_index];
		return seat_get_focus_inactive(seat, child);
	}

	// Surfaces
	struct sway_container *current = seat_get_active_child(seat, parent);

	return tiling_container_at(current, lx, ly, surface, sx, sy);
}

/**
 * container_at for a container with layout L_HORIZ or L_VERT.
 */
static struct sway_container *container_at_linear(struct sway_container *parent,
		double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	for (int i = 0; i < parent->children->length; ++i) {
		struct sway_container *child = parent->children->items[i];
		struct wlr_box box = {
			.x = child->x,
			.y = child->y,
			.width = child->width,
			.height = child->height,
		};
		if (wlr_box_contains_point(&box, lx, ly)) {
			return tiling_container_at(child, lx, ly, surface, sx, sy);
		}
	}
	return NULL;
}

static struct sway_container *floating_container_at(double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *output = root_container.children->items[i];
		for (int j = 0; j < output->children->length; ++j) {
			struct sway_container *workspace = output->children->items[j];
			struct sway_workspace *ws = workspace->sway_workspace;
			if (!workspace_is_visible(workspace)) {
				continue;
			}
			// Items at the end of the list are on top, so iterate the list in
			// reverse.
			for (int k = ws->floating->children->length - 1; k >= 0; --k) {
				struct sway_container *floater =
					ws->floating->children->items[k];
				struct wlr_box box = {
					.x = floater->x,
					.y = floater->y,
					.width = floater->width,
					.height = floater->height,
				};
				if (wlr_box_contains_point(&box, lx, ly)) {
					return tiling_container_at(floater, lx, ly,
							surface, sx, sy);
				}
			}
		}
	}
	return NULL;
}

struct sway_container *tiling_container_at(
		struct sway_container *con, double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	if (con->type == C_VIEW) {
		return container_at_view(con, lx, ly, surface, sx, sy);
	}
	if (!con->children->length) {
		return NULL;
	}

	switch (con->layout) {
	case L_HORIZ:
	case L_VERT:
		return container_at_linear(con, lx, ly, surface, sx, sy);
	case L_TABBED:
		return container_at_tabbed(con, lx, ly, surface, sx, sy);
	case L_STACKED:
		return container_at_stacked(con, lx, ly, surface, sx, sy);
	case L_FLOATING:
		sway_assert(false, "Didn't expect to see floating here");
		return NULL;
	case L_NONE:
		return NULL;
	}
	return NULL;
}

static bool surface_is_popup(struct wlr_surface *surface) {
	if (wlr_surface_is_xdg_surface(surface)) {
		struct wlr_xdg_surface *xdg_surface =
			wlr_xdg_surface_from_wlr_surface(surface);
		while (xdg_surface) {
			if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
				return true;
			}
			xdg_surface = xdg_surface->toplevel->parent;
		}
		return false;
	}

	if (wlr_surface_is_xdg_surface_v6(surface)) {
		struct wlr_xdg_surface_v6 *xdg_surface_v6 =
			wlr_xdg_surface_v6_from_wlr_surface(surface);
		while (xdg_surface_v6) {
			if (xdg_surface_v6->role == WLR_XDG_SURFACE_V6_ROLE_POPUP) {
				return true;
			}
			xdg_surface_v6 = xdg_surface_v6->toplevel->parent;
		}
		return false;
	}

	return false;
}

struct sway_container *container_at(struct sway_container *workspace,
		double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	if (!sway_assert(workspace->type == C_WORKSPACE, "Expected a workspace")) {
		return NULL;
	}
	struct sway_container *c;
	// Focused view's popups
	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *focus =
		seat_get_focus_inactive(seat, &root_container);
	if (focus && focus->type == C_VIEW) {
		container_at_view(focus, lx, ly, surface, sx, sy);
		if (*surface && surface_is_popup(*surface)) {
			return focus;
		}
		*surface = NULL;
	}
	// Floating
	if ((c = floating_container_at(lx, ly, surface, sx, sy))) {
		return c;
	}
	// Tiling
	if ((c = tiling_container_at(workspace, lx, ly, surface, sx, sy))) {
		return c;
	}
	return NULL;
}

void container_for_each_descendant_dfs(struct sway_container *container,
		void (*f)(struct sway_container *container, void *data),
		void *data) {
	if (!container) {
		return;
	}
	if (container->children)  {
		for (int i = 0; i < container->children->length; ++i) {
			struct sway_container *child = container->children->items[i];
			container_for_each_descendant_dfs(child, f, data);
		}
	}
	if (container->type == C_WORKSPACE)  {
		struct sway_container *floating = container->sway_workspace->floating;
		for (int i = 0; i < floating->children->length; ++i) {
			struct sway_container *child = floating->children->items[i];
			container_for_each_descendant_dfs(child, f, data);
		}
	}
	f(container, data);
}

void container_for_each_descendant_bfs(struct sway_container *con,
		void (*f)(struct sway_container *con, void *data), void *data) {
	list_t *queue = get_bfs_queue();
	if (!queue) {
		return;
	}

	if (queue == NULL) {
		wlr_log(WLR_ERROR, "could not allocate list");
		return;
	}

	list_add(queue, con);

	struct sway_container *current = NULL;
	while (queue->length) {
		current = queue->items[0];
		list_del(queue, 0);
		f(current, data);
		// TODO floating containers
		list_cat(queue, current->children);
	}
}

bool container_has_ancestor(struct sway_container *descendant,
		struct sway_container *ancestor) {
	while (descendant->type != C_ROOT) {
		descendant = descendant->parent;
		if (descendant == ancestor) {
			return true;
		}
	}
	return false;
}

static bool find_child_func(struct sway_container *con, void *data) {
	struct sway_container *child = data;
	return con == child;
}

bool container_has_child(struct sway_container *con,
		struct sway_container *child) {
	if (con == NULL || con->type == C_VIEW) {
		return false;
	}
	return container_find(con, find_child_func, child);
}

int container_count_descendants_of_type(struct sway_container *con,
		enum sway_container_type type) {
	int children = 0;
	if (con->type == type) {
		children++;
	}
	if (con->children) {
		for (int i = 0; i < con->children->length; i++) {
			struct sway_container *child = con->children->items[i];
			children += container_count_descendants_of_type(child, type);
		}
	}
	return children;
}

void container_damage_whole(struct sway_container *container) {
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *cont = root_container.children->items[i];
		if (cont->type == C_OUTPUT) {
			output_damage_whole_container(cont->sway_output, container);
		}
	}
}

static void update_title_texture(struct sway_container *con,
		struct wlr_texture **texture, struct border_colors *class) {
	if (!sway_assert(con->type == C_CONTAINER || con->type == C_VIEW,
			"Unexpected type %s", container_type_to_str(con->type))) {
		return;
	}
	struct sway_container *output = container_parent(con, C_OUTPUT);
	if (!output) {
		return;
	}
	if (*texture) {
		wlr_texture_destroy(*texture);
		*texture = NULL;
	}
	if (!con->formatted_title) {
		return;
	}

	double scale = output->sway_output->wlr_output->scale;
	int width = 0;
	int height = con->title_height * scale;

	cairo_t *c = cairo_create(NULL);
	get_text_size(c, config->font, &width, NULL, scale, config->pango_markup,
			"%s", con->formatted_title);
	cairo_destroy(c);

	cairo_surface_t *surface = cairo_image_surface_create(
			CAIRO_FORMAT_ARGB32, width, height);
	cairo_t *cairo = cairo_create(surface);
	cairo_set_source_rgba(cairo, class->background[0], class->background[1],
			class->background[2], class->background[3]);
	cairo_paint(cairo);
	PangoContext *pango = pango_cairo_create_context(cairo);
	cairo_set_antialias(cairo, CAIRO_ANTIALIAS_BEST);
	cairo_set_source_rgba(cairo, class->text[0], class->text[1],
			class->text[2], class->text[3]);
	cairo_move_to(cairo, 0, 0);

	pango_printf(cairo, config->font, scale, config->pango_markup,
			"%s", con->formatted_title);

	cairo_surface_flush(surface);
	unsigned char *data = cairo_image_surface_get_data(surface);
	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width);
	struct wlr_renderer *renderer = wlr_backend_get_renderer(
			output->sway_output->wlr_output->backend);
	*texture = wlr_texture_from_pixels(
			renderer, WL_SHM_FORMAT_ARGB8888, stride, width, height, data);
	cairo_surface_destroy(surface);
	g_object_unref(pango);
	cairo_destroy(cairo);
}

void container_update_title_textures(struct sway_container *container) {
	update_title_texture(container, &container->title_focused,
			&config->border_colors.focused);
	update_title_texture(container, &container->title_focused_inactive,
			&config->border_colors.focused_inactive);
	update_title_texture(container, &container->title_unfocused,
			&config->border_colors.unfocused);
	update_title_texture(container, &container->title_urgent,
			&config->border_colors.urgent);
	container_damage_whole(container);
}

void container_calculate_title_height(struct sway_container *container) {
	if (!container->formatted_title) {
		container->title_height = 0;
		return;
	}
	cairo_t *cairo = cairo_create(NULL);
	int height;
	get_text_size(cairo, config->font, NULL, &height, 1, config->pango_markup,
			"%s", container->formatted_title);
	cairo_destroy(cairo);
	container->title_height = height;
}

/**
 * Calculate and return the length of the tree representation.
 * An example tree representation is: V[Terminal, Firefox]
 * If buffer is not NULL, also populate the buffer with the representation.
 */
static size_t get_tree_representation(struct sway_container *parent, char *buffer) {
	size_t len = 2;
	switch (parent->layout) {
	case L_VERT:
		lenient_strcat(buffer, "V[");
		break;
	case L_HORIZ:
		lenient_strcat(buffer, "H[");
		break;
	case L_TABBED:
		lenient_strcat(buffer, "T[");
		break;
	case L_STACKED:
		lenient_strcat(buffer, "S[");
		break;
	case L_FLOATING:
		lenient_strcat(buffer, "F[");
		break;
	case L_NONE:
		lenient_strcat(buffer, "D[");
		break;
	}
	for (int i = 0; i < parent->children->length; ++i) {
		if (i != 0) {
			++len;
			lenient_strcat(buffer, " ");
		}
		struct sway_container *child = parent->children->items[i];
		const char *identifier = NULL;
		if (child->type == C_VIEW) {
			identifier = view_get_class(child->sway_view);
			if (!identifier) {
				identifier = view_get_app_id(child->sway_view);
			}
		} else {
			identifier = child->formatted_title;
		}
		if (identifier) {
			len += strlen(identifier);
			lenient_strcat(buffer, identifier);
		} else {
			len += 6;
			lenient_strcat(buffer, "(null)");
		}
	}
	++len;
	lenient_strcat(buffer, "]");
	return len;
}

void container_notify_subtree_changed(struct sway_container *container) {
	if (!container || container->type < C_WORKSPACE) {
		return;
	}
	free(container->formatted_title);
	container->formatted_title = NULL;

	size_t len = get_tree_representation(container, NULL);
	char *buffer = calloc(len + 1, sizeof(char));
	if (!sway_assert(buffer, "Unable to allocate title string")) {
		return;
	}
	get_tree_representation(container, buffer);

	container->formatted_title = buffer;
	if (container->type != C_WORKSPACE) {
		container_calculate_title_height(container);
		container_update_title_textures(container);
		container_notify_subtree_changed(container->parent);
	}
}

size_t container_titlebar_height() {
	return config->font_height + TITLEBAR_V_PADDING * 2;
}

void container_init_floating(struct sway_container *con) {
	if (!sway_assert(con->type == C_VIEW || con->type == C_CONTAINER,
			"Expected a view or container")) {
		return;
	}
	struct sway_container *ws = container_parent(con, C_WORKSPACE);
	int min_width, min_height;
	int max_width, max_height;

	if (config->floating_minimum_width == -1) { // no minimum
		min_width = 0;
	} else if (config->floating_minimum_width == 0) { // automatic
		min_width = 75;
	} else {
		min_width = config->floating_minimum_width;
	}

	if (config->floating_minimum_height == -1) { // no minimum
		min_height = 0;
	} else if (config->floating_minimum_height == 0) { // automatic
		min_height = 50;
	} else {
		min_height = config->floating_minimum_height;
	}

	if (config->floating_maximum_width == -1) { // no maximum
		max_width = INT_MAX;
	} else if (config->floating_maximum_width == 0) { // automatic
		max_width = ws->width * 0.6666;
	} else {
		max_width = config->floating_maximum_width;
	}

	if (config->floating_maximum_height == -1) { // no maximum
		max_height = INT_MAX;
	} else if (config->floating_maximum_height == 0) { // automatic
		max_height = ws->height * 0.6666;
	} else {
		max_height = config->floating_maximum_height;
	}

	if (con->type == C_CONTAINER) {
		con->width = max_width;
		con->height = max_height;
		con->x = ws->x + (ws->width - con->width) / 2;
		con->y = ws->y + (ws->height - con->height) / 2;
	} else {
		struct sway_view *view = con->sway_view;
		view->width = fmax(min_width, fmin(view->natural_width, max_width));
		view->height = fmax(min_height, fmin(view->natural_height, max_height));
		view->x = ws->x + (ws->width - view->width) / 2;
		view->y = ws->y + (ws->height - view->height) / 2;

		// If the view's border is B_NONE then these properties are ignored.
		view->border_top = view->border_bottom = true;
		view->border_left = view->border_right = true;

		container_set_geometry_from_floating_view(view->swayc);
	}
}

void container_set_floating(struct sway_container *container, bool enable) {
	if (container_is_floating(container) == enable) {
		return;
	}

	struct sway_seat *seat = input_manager_current_seat(input_manager);
	struct sway_container *workspace = container_parent(container, C_WORKSPACE);

	if (enable) {
		container_remove_child(container);
		container_add_child(workspace->sway_workspace->floating, container);
		container_init_floating(container);
		if (container->type == C_VIEW) {
			view_set_tiled(container->sway_view, false);
		}
	} else {
		// Returning to tiled
		if (container->scratchpad) {
			root_scratchpad_remove_container(container);
		}
		container_remove_child(container);
		struct sway_container *reference =
			seat_get_focus_inactive_tiling(seat, workspace);
		if (reference->type == C_VIEW) {
			reference = reference->parent;
		}
		container_add_child(reference, container);
		container->width = container->parent->width;
		container->height = container->parent->height;
		if (container->type == C_VIEW) {
			view_set_tiled(container->sway_view, true);
		}
		container->is_sticky = false;
	}

	container_end_mouse_operation(container);

	ipc_event_window(container, "floating");
}

void container_set_geometry_from_floating_view(struct sway_container *con) {
	if (!sway_assert(con->type == C_VIEW, "Expected a view")) {
		return;
	}
	if (!sway_assert(container_is_floating(con),
				"Expected a floating view")) {
		return;
	}
	struct sway_view *view = con->sway_view;
	size_t border_width = 0;
	size_t top = 0;

	if (!view->using_csd) {
		border_width = view->border_thickness * (view->border != B_NONE);
		top = view->border == B_NORMAL ?
			container_titlebar_height() : border_width;
	}

	con->x = view->x - border_width;
	con->y = view->y - top;
	con->width = view->width + border_width * 2;
	con->height = top + view->height + border_width;
}

bool container_is_floating(struct sway_container *container) {
	struct sway_container *workspace = container_parent(container, C_WORKSPACE);
	if (!workspace) {
		return false;
	}
	return container->parent == workspace->sway_workspace->floating;
}

void container_get_box(struct sway_container *container, struct wlr_box *box) {
	box->x = container->x;
	box->y = container->y;
	box->width = container->width;
	box->height = container->height;
}

/**
 * Translate the container's position as well as all children.
 */
void container_floating_translate(struct sway_container *con,
		double x_amount, double y_amount) {
	con->x += x_amount;
	con->y += y_amount;
	con->current.swayc_x += x_amount;
	con->current.swayc_y += y_amount;
	if (con->type == C_VIEW) {
		con->sway_view->x += x_amount;
		con->sway_view->y += y_amount;
		con->current.view_x += x_amount;
		con->current.view_y += y_amount;
	} else {
		for (int i = 0; i < con->children->length; ++i) {
			struct sway_container *child = con->children->items[i];
			container_floating_translate(child, x_amount, y_amount);
		}
	}
}

/**
 * Choose an output for the floating container's new position.
 *
 * If the center of the container intersects an output then we'll choose that
 * one, otherwise we'll choose whichever output is closest to the container's
 * center.
 */
static struct sway_container *container_floating_find_output(
		struct sway_container *con) {
	double center_x = con->x + con->width / 2;
	double center_y = con->y + con->height / 2;
	struct sway_container *closest_output = NULL;
	double closest_distance = DBL_MAX;
	for (int i = 0; i < root_container.children->length; ++i) {
		struct sway_container *output = root_container.children->items[i];
		struct wlr_box output_box;
		double closest_x, closest_y;
		container_get_box(output, &output_box);
		wlr_box_closest_point(&output_box, center_x, center_y,
				&closest_x, &closest_y);
		if (center_x == closest_x && center_y == closest_y) {
			// The center of the floating container is on this output
			return output;
		}
		double x_dist = closest_x - center_x;
		double y_dist = closest_y - center_y;
		double distance = x_dist * x_dist + y_dist * y_dist;
		if (distance < closest_distance) {
			closest_output = output;
			closest_distance = distance;
		}
	}
	return closest_output;
}

void container_floating_move_to(struct sway_container *con,
		double lx, double ly) {
	if (!sway_assert(container_is_floating(con),
			"Expected a floating container")) {
		return;
	}
	desktop_damage_whole_container(con);
	container_floating_translate(con, lx - con->x, ly - con->y);
	desktop_damage_whole_container(con);
	struct sway_container *old_workspace = container_parent(con, C_WORKSPACE);
	struct sway_container *new_output = container_floating_find_output(con);
	if (!sway_assert(new_output, "Unable to find any output")) {
		return;
	}
	struct sway_container *new_workspace =
		output_get_active_workspace(new_output->sway_output);
	if (old_workspace != new_workspace) {
		container_remove_child(con);
		container_add_child(new_workspace->sway_workspace->floating, con);
		arrange_windows(old_workspace);
		arrange_windows(new_workspace);
		workspace_detect_urgent(old_workspace);
		workspace_detect_urgent(new_workspace);
	}
}

void container_set_dirty(struct sway_container *container) {
	if (container->dirty) {
		return;
	}
	container->dirty = true;
	list_add(server.dirty_containers, container);
}

static bool find_urgent_iterator(struct sway_container *con,
		void *data) {
	return con->type == C_VIEW && view_is_urgent(con->sway_view);
}

bool container_has_urgent_child(struct sway_container *container) {
	return container_find(container, find_urgent_iterator, NULL);
}

void container_end_mouse_operation(struct sway_container *container) {
	struct sway_seat *seat;
	wl_list_for_each(seat, &input_manager->seats, link) {
		if (seat->op_container == container) {
			seat_end_mouse_operation(seat);
		}
	}
}

static void set_fullscreen_iterator(struct sway_container *con, void *data) {
	if (con->type != C_VIEW) {
		return;
	}
	if (con->sway_view->impl->set_fullscreen) {
		bool *enable = data;
		con->sway_view->impl->set_fullscreen(con->sway_view, *enable);
	}
}

void container_set_fullscreen(struct sway_container *container, bool enable) {
	if (container->is_fullscreen == enable) {
		return;
	}

	struct sway_container *workspace = container_parent(container, C_WORKSPACE);
	if (enable && workspace->sway_workspace->fullscreen) {
		container_set_fullscreen(workspace->sway_workspace->fullscreen, false);
	}

	container_for_each_descendant_dfs(container,
			set_fullscreen_iterator, &enable);

	container->is_fullscreen = enable;

	if (enable) {
		workspace->sway_workspace->fullscreen = container;
		container->saved_x = container->x;
		container->saved_y = container->y;
		container->saved_width = container->width;
		container->saved_height = container->height;

		struct sway_seat *seat;
		struct sway_container *focus, *focus_ws;
		wl_list_for_each(seat, &input_manager->seats, link) {
			focus = seat_get_focus(seat);
			if (focus) {
				focus_ws = focus;
				if (focus_ws->type != C_WORKSPACE) {
					focus_ws = container_parent(focus_ws, C_WORKSPACE);
				}
				if (focus_ws == workspace) {
					seat_set_focus(seat, container);
				}
			}
		}
	} else {
		workspace->sway_workspace->fullscreen = NULL;
		if (container_is_floating(container)) {
			container->x = container->saved_x;
			container->y = container->saved_y;
			container->width = container->saved_width;
			container->height = container->saved_height;
		} else {
			container->width = container->saved_width;
			container->height = container->saved_height;
		}
	}

	container_end_mouse_operation(container);

	ipc_event_window(container, "fullscreen_mode");
}

bool container_is_floating_or_child(struct sway_container *container) {
	do {
		if (container->parent && container->parent->layout == L_FLOATING) {
			return true;
		}
		container = container->parent;
	} while (container && container->type != C_WORKSPACE);

	return false;
}

bool container_is_fullscreen_or_child(struct sway_container *container) {
	do {
		if (container->is_fullscreen) {
			return true;
		}
		container = container->parent;
	} while (container && container->type != C_WORKSPACE);

	return false;
}

struct sway_container *container_wrap_children(struct sway_container *parent) {
	struct sway_container *middle = container_create(C_CONTAINER);
	middle->layout = parent->layout;
	while (parent->children->length) {
		struct sway_container *child = parent->children->items[0];
		container_remove_child(child);
		container_add_child(middle, child);
	}
	container_add_child(parent, middle);
	return middle;
}
